#include <TFile.h>
#include <TMath.h>
#include <TROOT.h>
#include <TTree.h>
#include <TVectorD.h>
#include <iostream>
#include <vector>

// calculate pseudorapidity from theta
Double_t calc_eta(Double_t theta) {
  return -1.0 * TMath::Log(TMath::Tan(theta / 2.0));
}

void ptcut(Double_t pTcut = 0.0, vector<TString> infiles = {"out.root"}) {
  // calculation parameters
  Double_t charge = 1e-3;  // charge in e
  Double_t data = 300.0;   // 300 fb^-1

  // calculate eta/phi acceptance
  Double_t det_loc = 43.1 * TMath::Pi() / 180.0;  // detector at 43.1 deg
  Double_t det_width = 1.0;                       // detector is 1x1 m
  Double_t det_dis = 33.0;                        // detector dist 33 m
  Double_t angle_sub = det_width / det_dis;       // angle subtended by detector
  Double_t phi_acceptance = angle_sub / (2 * TMath::Pi());  // phi acceptance
  // calculate eta with width extra_width times larger for good stats
  Double_t extra_width = 10.0;
  Double_t low_eta = calc_eta(det_loc + 0.5 * angle_sub * extra_width);
  Double_t high_eta = calc_eta(det_loc - 0.5 * angle_sub * extra_width);

  // initialize variables
  double event_sum = 0.0;
  double event_sumsq = 0.0;
  double sum_noetacut = 0.0;
  double sum_noetacutsq = 0.0;
  double sum_invtree_weight = 0.0;
  double sum_invtree_weight_err_sq = 0.0;

  // loop through input root files
  for (TString infile : infiles) {
    // read in input file
    TFile *EventFile = new TFile(infile, "READ");
    TTree *sourceTree = (TTree *)EventFile->Get("EventTree");
    unsigned int nentries = sourceTree->GetEntries();
    // Get event tree weight that includes cross section and number of events.
    // Cross section is in mb. Also gets its uncertainty (due to cross section
    // uncertainty output by Pythia).
    Double_t tree_weight = sourceTree->GetWeight();
    TVectorD *tree_err_vec = (TVectorD *)sourceTree->GetUserInfo()->At(0);
    Double_t tree_w_err = tree_err_vec[0][0];
    sum_invtree_weight += 1.0 / tree_weight;
    sum_invtree_weight_err_sq +=
        TMath::Power(tree_w_err / (tree_weight * tree_weight), 2);

    Double_t pT = 0.0;
    Double_t weight = 0.0;
    Double_t eta = 0.0;
    sourceTree->SetBranchAddress("pT", &pT);
    sourceTree->SetBranchAddress("eta", &eta);
    sourceTree->SetBranchAddress("weight", &weight);
    // count how many weighted mCP pass a pT cut (in GeV) and propagate errors
    for (unsigned int i = 0; i < nentries; i++) {
      sourceTree->GetEntry(i);
      Double_t abs_eta = TMath::Abs(eta);
      if (pT > pTcut) {
        // check if they pass eta cut for milliqan
        if (abs_eta > low_eta && abs_eta < high_eta) {
          event_sum += weight;
          event_sumsq += weight * weight;
        }
        sum_noetacut += weight;
        sum_noetacutsq += weight * weight;
      }
    }
  }

  Double_t event_sum_error = TMath::Sqrt(event_sumsq);
  Double_t sum_noetacut_error = TMath::Sqrt(sum_noetacutsq);

  // calculate detector acceptance and its uncertainty
  Double_t acceptance_reweight = phi_acceptance / extra_width * 0.5;
  Double_t acceptance_no_rw = event_sum / sum_noetacut;
  Double_t acceptance_no_rw_error =
      acceptance_no_rw *
      TMath::Sqrt(TMath::Power(event_sum_error / event_sum, 2) +
                  TMath::Power(sum_noetacut_error / sum_noetacut, 2));
  Double_t acceptance = acceptance_no_rw * acceptance_reweight;
  Double_t acceptance_error = acceptance_no_rw_error * acceptance_reweight;

  Double_t sum_invtree_weight_error = TMath::Sqrt(sum_invtree_weight_err_sq);
  Double_t reweight_from_tree = 1.0 / sum_invtree_weight;
  Double_t reweight_from_tree_error =
      sum_invtree_weight_error / (sum_invtree_weight * sum_invtree_weight);

  Double_t event_sum_reweight = event_sum * reweight_from_tree;
  Double_t event_sum_reweight_error =
      event_sum_reweight *
      TMath::Sqrt(
          TMath::Power(event_sum_error / event_sum, 2) +
          TMath::Power(reweight_from_tree_error / reweight_from_tree, 2));

  // calculate final reweighting for events to calculate mCP seen
  double final_reweight = 1.0;
  final_reweight *= 1e-3;               // mb to b
  final_reweight *= 1e15;               // b to fb
  final_reweight *= data;               // data in fb^-1
  final_reweight *= charge * charge;    // multiply by millicharge^2
  final_reweight *= phi_acceptance;     // phi acceptance
  final_reweight *= 1.0 / extra_width;  // adjust for large eta width
  final_reweight *= 0.5;                // adjust for having both + and - eta

  // calculate mCP/event that pass the pT cut
  // output equiv num of events stat-wise if each event had weight 1
  cout << event_sum * event_sum / event_sumsq
       << " equivalent events pass pT cut of " << pTcut << " GeV" << endl;
  cout << "acceptance is: " << acceptance << "+-" << acceptance_error << endl;
  cout << event_sum_reweight * final_reweight << "+-"
       << event_sum_reweight_error * final_reweight << " mCP seen with " << data
       << " fb^-1 of data" << endl;
}
