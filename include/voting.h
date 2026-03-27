#ifndef GRUDA_VOTING_H
#define GRUDA_VOTING_H

#include <Arduino.h>
#include "wallet.h"

#define MAX_PROPOSALS 10

enum VoteChoice : uint8_t {
    VOTE_YES     = 1,
    VOTE_NO      = 2,
    VOTE_ABSTAIN = 3
};

struct Proposal {
    String   id;
    String   title;
    String   description;
    uint32_t yesCount;
    uint32_t noCount;
    uint32_t abstainCount;
    uint32_t expiresAt;       /* unix timestamp */
    bool     voted;           /* has this node already voted? */
    VoteChoice myVote;
};

struct VotingState {
    Proposal proposals[MAX_PROPOSALS];
    uint8_t  proposalCount;
    int8_t   selectedIndex;   /* currently selected proposal on screen */
    bool     loading;
};

/* Initialize voting system */
void voting_init(VotingState& state);

/* Fetch proposals from api.grudge-studio.com/grudachain/proposals */
bool voting_fetch_proposals(VotingState& state, const String& authToken);

/* Submit a signed vote */
bool voting_submit(const Proposal& proposal, VoteChoice choice,
                   const GrudaWallet& wallet, const String& authToken);

#endif /* GRUDA_VOTING_H */
