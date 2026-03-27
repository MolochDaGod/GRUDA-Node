#ifndef GRUDA_UI_SHELL_H
#define GRUDA_UI_SHELL_H

#include "alerts.h"
#include "config.h"
#include "grd17.h"
#include "treaty.h"
#include "voting.h"
#include "wallet.h"

struct GrudgeAccount;

void ui_shell_create();
void ui_shell_set_account(GrudgeAccount *acct);
void ui_shell_set_wifi(bool connected, int8_t rssi);
void ui_shell_set_grudge_id(const char *id);
void ui_shell_set_uptime(uint32_t seconds);
void ui_shell_update_tabs(const GrudaWallet &wallet,
                          const GRD17NodeState &nodeState,
                          const TreatyState &treatyState,
                          const VotingState &votingState,
                          const AlertState &alertState, float balance);

#endif
