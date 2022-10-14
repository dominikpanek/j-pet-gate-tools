#include <map>
#include <string>
#include <sstream>
#include <cassert>

#include "EventAnalysis.h"
#include "Hits.h"

using namespace std;

//================================================================================
// MATHEMATICAL AND ADDITIONAL FUNCTIONS
//================================================================================

// struct used for sorting vectors of hits (functor)

namespace event_analysis{

struct EntityComp {

  string property;
  EntityComp(string property) {this->property = property;}
  bool operator()(const Hit& h1, const Hit& h2) const {
    bool val = false;
    if(property == "TIME")
      val = h1.time < h2.time;
    else if (property == "EDEP")
      val = h1.edep < h2.edep;
    else if (property == "EVENTID")
      val = h1.eventID < h2.eventID;
    return val;
  }

};

void sort_hits(vector<Hit> &hits, string key) {

  sort(hits.begin(), hits.end(), EntityComp(key));

}

Hit merge_hits(const std::vector<Hit> &hits, const AveragingMethod winner = kCentroidWinnerEnergyWeightedFirstTime) {
  const unsigned int nHits = hits.size();
  /// WK: temporary checks for debugs to be switched off
  assert(nHits > 0); 
  if (nHits > 0) {
    auto eID = hits[0].eventID;
    auto vID = hits[0].volumeID;
    assert(std::all_of(hits.begin(), hits.end(), [eID](const Hit& h)->bool { return h.eventID == eID; }));
    assert(std::all_of(hits.begin(), hits.end(), [vID](const Hit& h)->bool { return h.volumeID == vID; }));
  } 
  /// WK: end
  Hit h;
  h.eventID = hits[0].eventID;
  h.volumeID = hits[0].volumeID;
  // Energy of single4 == sum of energies of hits:
  for (unsigned int i=0; i<nHits; i++) h.edep += hits[i].edep;

  switch (winner) {

  case kCentroidWinnerNaivelyWeighted: {
    for (unsigned int i = 0; i < nHits; i++) {
      h.time += hits[i].time;
      h.posX += hits[i].posX;
      h.posY += hits[i].posY;
      h.posZ += hits[i].posZ;
    }
    h.time /= nHits;
    h.posX /= nHits;
    h.posY /= nHits;
    h.posZ /= nHits;
    break;
  }

  case kCentroidWinnerEnergyWeighted: {
    for (unsigned int i = 0; i < nHits; i++) {
      h.time += hits[i].time * hits[i].edep;
      h.posX += hits[i].posX * hits[i].edep;
      h.posY += hits[i].posY * hits[i].edep;
      h.posZ += hits[i].posZ * hits[i].edep;
    }
    h.time /= h.edep;
    h.posX /= h.edep;
    h.posY /= h.edep;
    h.posZ /= h.edep;
    break;
  }

  case kCentroidWinnerEnergyWeightedFirstTime: {
    std::vector<double> times;
    for (unsigned int i = 0; i < nHits; i++)
      times.push_back(hits[i].time);
    unsigned int min_index = std::distance(
        times.begin(), std::min_element(times.begin(), times.end()));
    h.time = hits[min_index].time;
    for (unsigned int i = 0; i < nHits; i++) {
      h.posX += hits[i].posX * hits[i].edep;
      h.posY += hits[i].posY * hits[i].edep;
      h.posZ += hits[i].posZ * hits[i].edep;
    }
    h.posX /= h.edep;
    h.posY /= h.edep;
    h.posZ /= h.edep;
    break;
  }

  case kEnergyWinner: {
    std::vector<double> energies;
    for (unsigned int i = 0; i<nHits; i++) energies.push_back(hits[i].edep);
    unsigned int max_index = std::distance(
        energies.begin(), std::max_element(energies.begin(), energies.end()));
    h.time = hits[max_index].time;
    h.posX = hits[max_index].posX;
    h.posY = hits[max_index].posY;
    h.posZ = hits[max_index].posZ;
    break;
  }

  default: {
    /// should never happen
    assert(1 == 0);
    break;
  }
  }
  h.sourcePosX = hits[0].sourcePosX;
  h.sourcePosY = hits[0].sourcePosY;
  h.sourcePosZ = hits[0].sourcePosZ;
  h.nPhantomCompton = hits[0].nPhantomCompton;
  h.nCrystalCompton = hits[0].nCrystalCompton;
  return h;
}

std::tuple<int, int, std::vector<Hit>> select_coincident_hits(const vector<Hit> &hits, double compton_energy_threshold) 
{
  std::vector<Hit> selected_hits;

  int nb_above_noise_treshold = hits.size();
  for (unsigned int i=0; i<hits.size(); i++) {
    if (hits[i].edep> compton_energy_threshold) selected_hits.push_back(hits[i]);
  }
  int nb_above_compton_threshold = selected_hits.size();
  return std::make_tuple(nb_above_noise_treshold, nb_above_compton_threshold, selected_hits);
}

std::tuple<int, int, std::vector<Hit>> select_coincident_singles (const std::vector<Hit> &hits,
  double compton_energy_threshold, const std::string& systemType, const AveragingMethod averagingMethod = kCentroidWinnerEnergyWeightedFirstTime) {

  map<string, vector<Hit>> singles_tmp;
  for (unsigned int i=0; i<hits.size(); i++) {
    string ID;
    if (systemType == "scanner")
      ID = std::to_string(hits[i].volumeID);
    else if (systemType == "cylindricalPET")
      ID = std::to_string(hits[i].rsectorID) + '_' + std::to_string(hits[i].layerID);
    if (singles_tmp.find(ID) == singles_tmp.end()) {
      vector<Hit> tmp;
      tmp.push_back(hits[i]);
      singles_tmp[ID] = tmp;
    } else {
      singles_tmp[ID].push_back(hits[i]);
    }
  }

  vector<Hit> singles;
  map<string, vector<Hit>>::iterator it_tmp = singles_tmp.begin();
  while(it_tmp != singles_tmp.end()) {
    singles.push_back(merge_hits(it_tmp->second, averagingMethod));
    it_tmp++;
  }
  return select_coincident_hits(singles, compton_energy_threshold);
}

bool all_same_event(const vector<Hit>& hits) {
 unsigned int nhits = hits.size();
 if ( nhits == 0 ) {
  return false;
 }
 const int ref_id = hits.at(0).eventID;
 return std::all_of(hits.cbegin(), hits.cend(), [ref_id](Hit& hit){ return hit.eventID == ref_id; });
}

bool not_phantom_compton(const vector<Hit>& hits) {
 return std::all_of(hits.cbegin(), hits.cend(), [](Hit& hit){ return hit.nPhantomCompton == 0; });
}

bool all_crystal_compton(const vector<Hit>& hits) {
 return std::all_of(hits.cbegin(), hits.cend(), [](Hit& hit){ return hit.nCrystalCompton == 1; });
}

EventType verify_type_of_coincidence(const std::vector<Hit>& hits) {
 if ( all_same_event(hits) ) {
  //true, phantom-scattered and detector-scattered
  if ( not_phantom_compton(hits) ) {
   if ( all_crystal_compton(hits) ) {
    return kTrue;
   }
   return kDetectorScattered;
  }
  return kPhantomScattered;
 }
 //otherwise --> accidental
 return kAccidental;
}

// based on ComputeKindGATEEvent from
// https://github.com/JPETTomography/castor/blob/castor-3.1.1/src/management/gDataConversionUtilities.cc
EventType verify_type_of_coincidence_castor(const Hit &h1, const  Hit &h2) {

  if (h1.eventID != h2.eventID)
    // random
    return kAccidental;
  else
  {
    if (h1.nPhantomCompton == 0 && h2.nPhantomCompton == 0 &&
         h1.nPhantomRayleigh == 0 && h2.nPhantomRayleigh == 0 &&
         h1.nCrystalCompton == 1 && h2.nCrystalCompton == 1 &&
         h1.nCrystalRayleigh == 0 && h2.nCrystalRayleigh == 0)
        //true
        return kTrue;
    else
    {
      if (h1.nPhantomCompton == 0 && h2.nPhantomCompton == 0 &&
          h1.nPhantomRayleigh == 0 && h2.nPhantomRayleigh == 0 &&
          (h1.nCrystalCompton > 1 || h2.nCrystalCompton > 1 ||
          h1.nCrystalRayleigh > 0 || h2.nCrystalRayleigh > 0))
         //detector scat
         return kDetectorScattered;
      if (h1.nPhantomCompton == 1 || h2.nPhantomCompton == 1 ||
          h1.nPhantomRayleigh == 1 || h2.nPhantomRayleigh == 1)
         //single scat
          return kPhantomScattered;
      if (h1.nPhantomCompton > 1 || h2.nPhantomCompton > 1 ||
          h1.nPhantomRayleigh > 1 || h2.nPhantomRayleigh > 1)
         //multi scat
          return kPhantomScattered;
    }
  }
  //unknown
  return kUnspecified;
}
// Code checking the type of 3 gamma events. Gammas are assigned to four classes. True, Phantom Scattered, Detector
// Scattered and Accidental (Random).
EventType verify_type_of_triple_coincidence(const std::vector<Hit>& hits) {
 if ( all_same_event(hits) ) {
  //true, phantom-scattered and detector-scattered
  if ( not_phantom_compton(hits) ) {
   if ( all_crystal_compton(hits) ) {
    return kTrue;
   }
   return kDetectorScattered;
  }
  return kPhantomScattered;
 }
 //otherwise --> accidental
 return kAccidental;
}


std::tuple<bool, std::vector<Hit>> check_gamma_type(const vector<Hit> &hits, double prompt_energy_threshold) 
{
  std::vector<Hit> triple_hits;

  for (unsigned int i=0; i<hits.size(); i++) {
    if (hits[i].edep <= prompt_energy_threshold) triple_hits.push_back(hits[i]);
  }
 int prompt_multiplicity = 0;
 for (unsigned int i=0; i<hits.size(); i++) {
    if (hits[i].edep > prompt_energy_threshold) {
 triple_hits.push_back(hits[i]);
 prompt_multiplicity++;
 }
  }
 if(prompt_multiplicity == 1) {
 bool isGood = true;
 }
 else {
 bool isGood = false;
 }
  return std::make_tuple(isGood, triple_hits);
 }

}

//================================================================================
// PRINTING
//================================================================================

void print_coincidence(const Hit& h1, const Hit& h2) {

  cout.setf(ios::fixed);

  cout.precision(2);
  cout << h1.posX/10. << "\t" << h1.posY/10. << "\t" << h1.posZ/10. << "\t";
  cout.precision(1);
  cout << h1.time << "\t";

  cout.precision(2);
  cout << h2.posX/10. << "\t" << h2.posY/10. << "\t" << h2.posZ/10. << "\t";
  cout.precision(1);
  cout << h2.time << "\t";

  cout << h1.volumeID << "\t";
  cout << h2.volumeID << "\t";

  cout.precision(2);
  cout << h1.edep << "\t";
  cout << h2.edep << "\t";

  cout << verify_type_of_coincidence_castor(h1, h2) << "\t";

  cout.precision(2);
  cout << h1.sourcePosX/10. << "\t" << h1.sourcePosY/10. << "\t" << h1.sourcePosZ/10. << "\t";
  cout << h2.sourcePosX/10. << "\t" << h2.sourcePosY/10. << "\t" << h2.sourcePosZ/10. << "\t";

  cout << h1.eventID << "\t";
  cout << h2.eventID << "\t";
  cout << h1.nPhantomCompton << "\t";
  cout << h2.nPhantomCompton << "\t";
  cout << h1.nPhantomRayleigh << "\t";
  cout << h2.nPhantomRayleigh << "\t";
  cout << h1.nCrystalCompton << "\t";
  cout << h2.nCrystalCompton << "\t";
  cout << h1.nCrystalRayleigh << "\t";
  cout << h2.nCrystalRayleigh << "\t";

  cout << h1.rsectorID << "\t";
  cout << h2.rsectorID << "\t";
  cout << h1.moduleID << "\t";
  cout << h2.moduleID << "\t";
  cout << h1.submoduleID << "\t";
  cout << h2.submoduleID << "\t";
  cout << h1.crystalID << "\t";
  cout << h2.crystalID << "\t";
  cout << h1.layerID << "\t";
  cout << h2.layerID << endl;

}

void print_coincidences(const std::vector<Hit>& hits) {

  for (unsigned int i=0; i<hits.size()-1; i++) {
    print_coincidence(hits[i], hits[i+1]);
  }

}

// Code printing specific information about the hits. The information include X, Y and Z position; detection time,
// energy deposition etc.
void print_triple_coincidences(const std::vector<Hit>& hits, double PROMPT_E_TH) {

 assert(hits.size() == 3);

 std::vector<Hit> triple_hits;
 bool isGood;
 std::tie(isGood, triple_hits) = check_gamma_type(hits, PROMPT_E_TH);

 if(isGood){

  cout.setf(ios::fixed);

  Hit h1 = triple_hits[0];
  Hit h2 = triple_hits[1];
  Hit h3 = triple_hits[2];

  cout.precision(2);
  cout << h1.posX/10. << "\t" << h1.posY/10. << "\t" << h1.posZ/10. << "\t";
  cout.precision(1);
  cout << h1.time << "\t";

  cout.precision(2);
  cout << h2.posX/10. << "\t" << h2.posY/10. << "\t" << h2.posZ/10. << "\t";
  cout.precision(1);
  cout << h2.time << "\t";

  cout << h1.volumeID << "\t";
  cout << h2.volumeID << "\t";

  cout.precision(2);
  cout << h1.edep << "\t";
  cout << h2.edep << "\t";

  cout << verify_type_of_triple_coincidence(h1, h2, h3) << "\t";

  cout.precision(2);
  cout << h1.sourcePosX/10. << "\t" << h1.sourcePosY/10. << "\t" << h1.sourcePosZ/10. << "\t";
  cout << h2.sourcePosX/10. << "\t" << h2.sourcePosY/10. << "\t" << h2.sourcePosZ/10. << endl;

  cout.precision(2);
  cout << h3.posX/10. << "\t" << h3.posY/10. << "\t" << h3.posZ/10. << "\t";
  cout.precision(1);
  cout << h3.time << "\t";
  cout << h3.volumeID << "\t";
  cout.precision(2);
  cout << h3.edep << "\t";
  cout.precision(2);
  cout << h3.sourcePosX/10. << "\t" << h3.sourcePosY/10. << "\t" << h3.sourcePosZ/10. << "\t";

}

}

//================================================================================
// MAIN ANALYSIS FUNCTION
//================================================================================

void analyze_event(const vector<Hit> &hits, const bool hits_are_singles, const bool triple_coincidence)
{

  double COMPTON_E_TH = atof(getenv("GOJA_COMPTON_E_TH"))*1e3;
  double PROMPT_E_TH = atof(getenv(“GOJA_PROMPT_E_TH”))*1e3;
  int MAX_N = int(atof(getenv("GOJA_MAX_N")));
  int MAX_N0 = int(atof(getenv("GOJA_MAX_N0")));
  int N = 0;
  int N0 = 0;
  const auto averagingMethod = AveragingMethod(int(atof(getenv("GOJA_AVERAGING_METHOD"))));
  const string systemType = string(getenv("GOJA_SYSTEM_TYPE"));

  sort_hits(hits, "TIME");

  std::vector<Hit> selected_hits;
  if (hits_are_singles) {
    std::tie(N0, N, selected_hits) = select_coincident_singles(hits, COMPTON_E_TH, systemType, averagingMethod);
  }
  else {
    std::tie(N0, N, selected_hits) = select_coincident_hits(hits, COMPTON_E_TH);
  }

  // If number of selected singles/hits equals maximum number of events above the fixed energy threshold in the coincidence window
  // and number of all hits in the event is equal or smaller than maximum number of events above the noise energy threshold in the coincidence window
  // then print the coincidence:
  if (N==MAX_N and N0<=MAX_N0) print_coincidences(selected_hits);
  //if (N>=2 and N<=MAX_N and N0<=MAX_N0) print_coincidences(selected_hits);

  if (triple_coincidences) {
  if (N==MAX_N and N0<=MAX_N0) {print_coincidences(selected_hits);}
  }
  else {
  if (N==MAX_N and N0<=MAX_N0) {print_triple_coincidences(selected_hits)};
  }

  if (DEBUG) {
    cout.setf(ios::fixed);
    cout.precision(1);
    for (unsigned int i=0; i<hits.size(); i++) {
      double t = hits[i].time;
      if (i==0) cout << endl;
      cout.precision(1);
      cout << t << "\t";
      cout.precision(2);
      cout << hits[i].edep;
      if (i>0) {
        cout.precision(1);
        cout << "\t" << (hits[i].time-hits[i-1].time);
      }
      if (i==(hits.size()-1)) {
        cout.precision(1);
        cout << "\t" << (hits[i].time-hits[0].time) << endl;
        cout << N << "\t" << N0 << endl;
      }
      cout << endl;
    }
  }

}

void RunTests()
{
  const double epsilon = 0.0001;

  Hit h1;
  h1.edep = 10;
  h1.time = 12;
  h1.posX = 1; h1.posY = 2; h1.posZ = 3;

  Hit h2;
  h2.edep = 80;
  h2.time = 22;
  h2.posX = 2; h2.posY = 3; h2.posZ = 4;

  Hit h3;
  h3.edep = 50;
  h3.time = 32;
  h3.posX = 7; h3.posY = 4; h3.posZ = 5;

  std::vector<Hit> hits = {h1, h2, h3};

  for (auto& h : hits)
  {
    h.eventID = 10;
    h.volumeID = 9;
  }

  auto single1 = merge_hits(hits, kCentroidWinnerNaivelyWeighted);
  assert(std::abs(single1.time-22) < epsilon); // (12+22+32)/3 = 22
  assert(std::abs(single1.posX-3.333333333) < epsilon); // (1+2+7)/2 = 3.333333333
  assert(std::abs(single1.edep - 140) < epsilon); // Energy of single == sum of energies of hits: 10+80+50 = 140

  auto single2 = merge_hits(hits, kCentroidWinnerEnergyWeighted);
  assert(std::abs(single2.time-24.85714286) < epsilon); // (10*12+80*22+50*32)/140 = 24.85714286
  assert(std::abs(single2.posX-3.714285714) < epsilon); // (10*1+80*2+50*7)/140 = 3.714285714
  assert(std::abs(single2.edep - 140) < epsilon); // Energy of single == sum of energies of hits: 10+80+50 = 140

  auto single3 = merge_hits(hits, kCentroidWinnerEnergyWeightedFirstTime);
  assert(std::abs(single3.time-12) < epsilon); // min(12,22,32) = 12
  assert(std::abs(single3.posX-3.714285714) < epsilon); // (10*1+80*2+50*7)/140 = 3.714285714
  assert(std::abs(single3.edep - 140) < epsilon); // Energy of single == sum of energies of hits: 10+80+50 = 140

  auto single4 = merge_hits(hits, kEnergyWinner);
  assert(std::abs(single4.time-22) < epsilon); // max(10,80,50) = 80 => single takes attributes of hit h2
  assert(std::abs(single4.posX-2) < epsilon); // max(10,80,50) = 80 => single takes attributes of hit h2
  assert(single4.eventID == 10); // 10 is a value set manually
  assert(single4.volumeID == 9); // 9 is a value set manually
  assert(std::abs(single4.edep - 140) < epsilon); // Energy of single == sum of energies of hits: 10+80+50 = 140
}
