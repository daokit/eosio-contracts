#ifndef BANK_H
#define BANK_H

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>

#include "eosiotoken.hpp"
#include "common.hpp"

using namespace eosio;
using std::string;
using std::map;

class Bank {

    public:

        struct [[eosio::table, eosio::contract("dao") ]] Config 
        {
            // required configurations:
            // names : telos_decide_contract, reward_token_contract, seeds_token_contract, last_ballot_id
            // ints  : voting_duration_sec
            map<string, name>          names             ;
            map<string, string>        strings           ;
            map<string, asset>         assets            ;
            map<string, time_point>    time_points       ;
            map<string, uint64_t>      ints              ;
            map<string, transaction>   trxs              ;
            map<string, float>         floats            ;
        };

        typedef singleton<"config"_n, Config> config_table;

        struct [[eosio::table, eosio::contract("dao") ]] Period
        {
            uint64_t         period_id               ;
            time_point       start_date              ;
            time_point       end_date                ;

            uint64_t        primary_key()  const { return period_id; }
        };

        struct [[eosio::table, eosio::contract("dao") ]] Payment
        {
            uint64_t        payment_id              ;
            time_point      payment_date            ;
            uint64_t        period_id               = 0;
            uint64_t        assignment_id           = -1;
            name            recipient               ;
            asset           amount                  ;
            string          memo                    ;

            uint64_t        primary_key()   const { return payment_id; }
            uint64_t        by_period ()    const { return period_id; }
            uint64_t        by_recipient()  const { return recipient.value; }
            uint64_t        by_assignment() const { return assignment_id; }
        };

        typedef multi_index<"periods"_n, Period> period_table;

        typedef multi_index<"payments"_n, Payment,
            indexed_by<"byperiod"_n, const_mem_fun<Payment, uint64_t, &Payment::by_period>>,
            indexed_by<"byrecipient"_n, const_mem_fun<Payment, uint64_t, &Payment::by_recipient>>,
            indexed_by<"byassignment"_n, const_mem_fun<Payment, uint64_t, &Payment::by_assignment>>
        > payment_table;

        struct [[eosio::table, eosio::contract("dao") ]] Debug
        {
            uint64_t    debug_id;
            string      notes;
            time_point  created_date = current_time_point();
            uint64_t    primary_key()  const { return debug_id; }
        };

      typedef multi_index<"debugs"_n, Debug> debug_table;


      void debug (const string& notes) {
         debug_table d_t (contract, contract.value);
         d_t.emplace (contract, [&](auto &d) {
            d.debug_id = d_t.available_primary_key();
            d.notes = notes;
         });
      }


        name                contract;
        period_table        period_t;
        payment_table       payment_t;
        config_table        config_s;

        Bank (const name& contract):
            contract (contract),
            payment_t (contract, contract.value),
            period_t (contract, contract.value),
            config_s (contract, contract.value) {}

       void reset () {
            require_auth (contract);
            
            auto pay_itr = payment_t.begin();
            while (pay_itr != payment_t.end()) {
                pay_itr = payment_t.erase (pay_itr);
            }
        }

        void remove_periods (const uint64_t& begin_period_id, 
                           const uint64_t& end_period_id) {
            require_auth (contract);

            auto p_itr = period_t.find (begin_period_id);
            check (p_itr != period_t.end(), "Begin period ID not found: " + std::to_string(begin_period_id));

            while (p_itr->period_id <= end_period_id) {
                p_itr = period_t.erase (p_itr);
            }
        }

        void reset_periods() {
            require_auth (contract);
            auto per_itr = period_t.begin();
            while (per_itr != period_t.end()) {
                per_itr = period_t.erase (per_itr);
            }
        }
                          
        void makepayment (const uint64_t& period_id, 
                            const name& recipient, 
                            const asset& quantity, 
                            const string& memo, 
                            const uint64_t& assignment_id,
                            const uint64_t& bypass_escrow) {

            if (quantity.amount == 0) {
                return;
            }

            debug ("Making payment to recipient: " + recipient.to_string() + ", quantity: " + quantity.to_string());
    
            config_table      config_s (contract, contract.value);
   	        Config c = config_s.get_or_create (contract, Config());   

            if (quantity.symbol == common::S_VOTE) {
                action(
                    permission_level{contract, "active"_n},
                    c.names.at("telos_decide_contract"), "mint"_n,
                    std::make_tuple(recipient, quantity, memo))
                .send();
            } else {   // handles USD and REWARD
                // need to add steps in here about the deferments         
                issuetoken (c.names.at("reward_token_contract"), recipient, quantity, memo );
            } 
           
            payment_t.emplace (contract, [&](auto &p) {
                p.payment_id    = payment_t.available_primary_key();
                p.payment_date  = current_block_time().to_time_point();
                p.period_id     = period_id;
                p.assignment_id = assignment_id;
                p.recipient     = recipient;
                p.amount        = quantity;
                p.memo          = memo;
            });
        }

        void addperiod (const time_point& start_date, const time_point& end_date) {

            period_t.emplace (contract, [&](auto &p) {
                p.period_id     = period_t.available_primary_key();
                p.start_date    = start_date;
                p.end_date      = end_date;
            });
        }

        void issuetoken(const name& token_contract,
                        const name& to,
                        const asset& token_amount,
                        const string& memo)
        {
            string debug_str = "";
            debug_str = debug_str + "Issue Token Event; ";
            debug_str = debug_str + "    Token Contract  : " + token_contract.to_string() + "; ";
            debug_str = debug_str + "    Issue To        : " + to.to_string() + "; ";
            debug_str = debug_str + "    Issue Amount    : " + token_amount.to_string() + ";";
            debug_str = debug_str + "    Memo            : " + memo + ".";

            action(
                permission_level{contract, "active"_n},
                token_contract, "issue"_n,
                std::make_tuple(contract, token_amount, memo))
            .send();

            action(
                permission_level{contract, "active"_n},
                token_contract, "transfer"_n,
                std::make_tuple(contract, to, token_amount, memo))
            .send();

            debug (debug_str);
        }

        bool holds_hypha (const name& account) 
        {
            config_table      config_s (contract, contract.value);
   	        Config c = config_s.get_or_create (contract, Config());   

            eosiotoken::accounts a_t (c.names.at("reward_token_contract"), account.value);
            auto a_itr = a_t.find (common::S_REWARD.code().raw());
            if (a_itr == a_t.end()) {
                return false;
            } else if (a_itr->balance.amount > 0) {
                return true;
            } else {
                return false;
            }
        }

};

#endif