#ifndef _AUDIT_H
#define _AUDIT_H

#include "model.h"

struct AuditSpec{
	double asn;
	int winner;
	int loser;

	Ints eliminated;
	bool wonly;
};

struct Result{
	int polls;
	int remaining_hypotheses;
};

typedef std::vector<AuditSpec> Audits;

double EstimateSampleSize(const Ballots &ballots, const Candidates &cand, 
	double logrlimit, const Ints &tail, AuditSpec &audit, bool alglog);
double EstimateSampleSizeWL(const Ballots &ballots, const Candidates &cand, 
	double logrlimit, int cw, int cl);

Result RunSingleWinnerLoserAudit(const Ballots &rep_ballots, 
	const Ballots &act_ballots,
	const Candidates &cand, double rlimit, int winner, 
	int loser, const Ints &plist);

Result RunAudit(const Ballots &rep_ballots, const Ballots &act_ballots,
	const Candidates &candidates, double rlimit, const Ints &winners, 
	const Ints &losers,const Ints &plist); 

bool LoadAudits(const char *path, Audits &audits,
	const Candidates &candidates, const Config &config);

#endif
