/*
    Copyright (C) 2018-2019  Michelle Blom

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "audit.h"
#include<fstream>
#include<iostream>
#include<cmath>

const double EPSILON = 0.0000001;

using namespace std;
typedef boost::char_separator<char> boostcharsep;
void PrintBallot(const Ballot &b){
	for(int i = 0; i < b.prefs.size(); ++i){
		cout << b.prefs[i] << " ";
	}
}

bool LoadAudits(const char *path, Audits &audits,
	const Candidates &candidates, const Config &config){
	try
	{
		ifstream infile(path);

		cout << "Reading Audits" << endl;
		boostcharsep sp(",");

		string line;	
		while(getline(infile, line))
		{
			if(!(boost::starts_with(line, "EO")||
				boost::starts_with(line,"WO"))){
				continue;
			}
	
			vector<string> columns;
			Split(line, sp, columns);

			AuditSpec spec;
			spec.wonly = false;

			if(columns[0] == "WO"){
				spec.wonly = true;
			}

			int winner = ToType<int>(columns[2]);
			int loser = ToType<int>(columns[4]);

			spec.winner = config.id2index.find(winner)->second;
			spec.loser = config.id2index.find(loser)->second;

			for(int k = 6; k < columns.size(); ++k){
				int elim = ToType<int>(columns[k]);
				spec.eliminated.push_back(config.id2index.find(elim)->second);
			} 
			audits.push_back(spec);
		}

		cout << "Finished reading audits" << endl;
		infile.close();
	}
	catch(exception &e)
	{
		throw e;
	}
	catch(STVException &e)
	{
		throw e;
	}
	catch(...)
	{
		cout << "Unexpected error reading in audits." << endl;
		return false;
	}

	return true;
}

int GetFirstCandidateStanding(const Candidates &cand, const Ints &prefs){
	for(Ints::const_iterator it = prefs.begin(); it != prefs.end(); ++it){
		if(cand[*it].standing == 1){
			return *it;
		}
	}
	return -1;
}


int GetFirstCandidateIn(const Ints &prefs, const Ints &relevant){
	for(int k = 0; k < prefs.size(); ++k){
		if(find(relevant.begin(), relevant.end(),prefs[k]) != relevant.end()){
			return prefs[k];
		}
	}
	return -1;
}
double EstimateSampleSizeWL(const Ballots &ballots, const Candidates &cand, 
	double logrlimit, int cw, int cl){
	double total_votes_present = ballots.size();
	double total_votes_loser = 0;
	double total_votes_winner = cand[cw].sim_votes;
	Ints relevant;
	relevant.push_back(cw);
	relevant.push_back(cl);

	const Ints &bwhereappear = cand[cl].ballots_where_appear;
	for(int k = 0; k < bwhereappear.size(); ++k){
		const Ballot &bt = ballots[bwhereappear[k]];

		int nextcand = GetFirstCandidateIn(bt.prefs, relevant);
		if(nextcand == cl){
			total_votes_loser += 1;
		}
	}

	if(total_votes_winner <= total_votes_loser){
		return -1;
	}

	double pl = total_votes_loser / total_votes_present;
	double pw = total_votes_winner / total_votes_present;
	double swl = (pw == 0) ? 0 : pw / (pl + pw);
	if(pl + pw == 0){
		return -1;
	}
	const double log2swl = log(2*swl);
	double candasn = ceil((logrlimit+0.5*log2swl)/(double)(pw*log2swl+pl*log(2-2*swl)));

	return candasn/total_votes_present; 
}

double EstimateSampleSize(const Ballots &ballots, const Candidates &cand, 
	double logrlimit, const Ints &tail, AuditSpec &best_audit, bool alglog){
	// Compute ASN to show that tail[0] beats one of tail[1..n]
	double totalvotes = ballots.size();
	const int loser = tail[0];
	const int tsize = tail.size();
	Doubles total_votes_each(tsize, 0);
	
	for(int b = 0; b < ballots.size(); ++b){
		const Ballot &bt = ballots[b];
		int nextcand = GetFirstCandidateIn(bt.prefs, tail);
		for(int k = 0; k < tsize; ++k){
			if(nextcand == tail[k]){
				total_votes_each[k] += 1;
			}
		}
	}

	for(int k = 0; k < tsize; ++k){
		total_votes_each[k]/=totalvotes;
	}

	best_audit.eliminated.clear();
	for(int k = 0; k < cand.size(); ++k){
		if(find(tail.begin(), tail.end(), k) == tail.end()){
			best_audit.eliminated.push_back(k);
		}
	}


	double smallest = -1;
	for(int i = 1; i < tsize; ++i){
		// loser is the "winner" and tail[i] is the "loser"
		const int taili = tail[i];
		double pw = total_votes_each[0];
		double pl = total_votes_each[i];

		if(pl >= pw || pl + pw == 0){
			continue;
		}

		double swl = (pw == 0) ? 0 : pw / (pl + pw);
		double candasn = -1;
		const double log2swl = log(2*swl);
		candasn = ceil((logrlimit+0.5*log2swl)/(double)(pw*log2swl+pl*log(2-2*swl)))/totalvotes;

		if(smallest == -1 || candasn < smallest){
			best_audit.asn = candasn;
			best_audit.winner = loser;
			best_audit.loser = taili;
			smallest = candasn;
		}
	}

	return smallest;
}

Result RunSingleWinnerLoserAudit(const Ballots &rep_ballots, 
	const Ballots &act_ballots, const Candidates &cand,
	double rlimit, int winner, int loser, const Ints &plist){
	
	double total_votes_loser = 0;
	double total_votes_winner = cand[winner].sim_votes;

	Ints relevant;
	relevant.push_back(winner);
	relevant.push_back(loser);

	for(Ballots::const_iterator bt = rep_ballots.begin(); bt != rep_ballots.end(); ++bt){
		int nextcand = GetFirstCandidateIn(bt->prefs, relevant);
		if(nextcand == loser){
			total_votes_loser += 1;
		}
	}
	
	const double frlimit = 1.0/rlimit;
	const double swl = total_votes_winner / (double)(total_votes_winner + total_votes_loser);
 
	double twl = 1;
	int polls = 0;
	Result r;
	r.remaining_hypotheses = 1;

	for(int p = 0; p < plist.size(); ++p){
		++polls;
	
		const Ballot &b = act_ballots[plist[p]];
		const int c = GetFirstCandidateIn(b.prefs, relevant);

		if(c == winner){
			twl *= swl/0.5;
		}
		else if(c == loser){
			twl *= (1-swl)/0.5;
		}

		if(twl >= frlimit){
			r.remaining_hypotheses = 0;
			break;
		}
	}	
	r.polls = polls;
	return r;
}

Result RunAudit(const Ballots &rep_ballots, const Ballots &act_ballots, 
	const Candidates &cand, double rlimit, const Ints &winners, 
	const Ints &losers,const Ints &plist){
	const int max = rep_ballots.size();
	const int ncand = cand.size();
	const int nwinners = winners.size();	
	const int nlosers = losers.size();
	
	Ints action(ncand, 0);

	// Initialse T_{wl} statistics for every winner w and loser l (set to 1)
	Doubles2d swl_mult;
	Doubles2d tstats;
	double total_votes_present = 0;
	for(int i = 0; i < ncand; ++i){
		tstats.push_back(Doubles(ncand, 1));
		swl_mult.push_back(Doubles(ncand, 0));
		if(cand[i].standing == 1){
			total_votes_present += cand[i].sim_votes;
		}
	}

	// Compute s_{wl} each winner-loser pair
	for(Ints::const_iterator wt=winners.begin(); wt!=winners.end(); ++wt){
		action[*wt] = 1;
		for(Ints::const_iterator lt=losers.begin(); lt!=losers.end(); ++lt){
			//cout << " Simulated votes for winner " << *wt << " " << cand[*wt].sim_votes
			//	<< " and loser " << *lt << " " << cand[*lt].sim_votes << endl;
			double swl = cand[*wt].sim_votes/(double)(cand[*wt].sim_votes+cand[*lt].sim_votes);
			swl_mult[*wt][*lt] =  swl/0.5;
			swl_mult[*lt][*wt] = (1 - swl)/0.5;
			//cout << "swl for w = " << *wt << " and l = " << *lt << " is " << swl << endl;
		}
	}

	const double frlimit = 1.0/rlimit;
	//cout << "Once statistics >= " << frlimit << ", reject" << endl;
		
	int polls = 0;
	int rejects = 0;

	// When we reject a null hypothesis the corresponding Twl is set to -1,
	// so that we do not further manipulate that statistic with later polls.
	while(polls < max && rejects < nwinners*nlosers){
		int rbidx = plist[polls];

		const Ballot &b = act_ballots[rbidx];

		cout << "Ballot polled ";
		PrintBallot(b);
		cout << endl;

		const int c = GetFirstCandidateStanding(cand, b.prefs);
		if(c == -1){//ballot counts for a previously eliminated candidate
			++polls;
			continue;
		}

		if(action[c] == 1){ //winner
			for(Ints::const_iterator lt=losers.begin(); lt!=losers.end(); ++lt){
				if(tstats[c][*lt] != -1){
					cout << "winner Twl["<<c<<"]["<<*lt<<"] : " << tstats[c][*lt] << " -> ";
					tstats[c][*lt] *= swl_mult[c][*lt];	
					cout << tstats[c][*lt] << endl;
					if(tstats[c][*lt] >= frlimit){
						tstats[c][*lt] = -1;
						cout << "Rejecting hypothesis " <<  "Twl["<<c<<"]["<<*lt<<"]" << endl; 
						++rejects;
					}
				}
			}
		}
		else{//loser
			for(Ints::const_iterator wt=winners.begin(); wt!=winners.end(); ++wt){
				if(tstats[*wt][c] != -1){
					cout << "loser Twl["<<*wt<<"]["<<c<<"] : " << tstats[*wt][c] << " -> ";
					tstats[*wt][c] *= swl_mult[c][*wt];
					cout << tstats[*wt][c] << endl;
					if(tstats[*wt][c] >= frlimit){
						tstats[*wt][c] = -1;
						cout << "Rejecting hypothesis " <<  "Twl["<<*wt<<"]["<<c<<"]" << endl; 
						++rejects;
					}
				}
			}
		}		
		++polls;
	}

	Result r;
	r.polls = polls;
	r.remaining_hypotheses = nwinners*nlosers - rejects;
	return r;	
}
