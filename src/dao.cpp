#include <dao.hpp>

void dao::addmember (const name& member) {
	require_auth (get_self());
	member_table m_t (get_self(), get_self().value);
	auto m_itr = m_t.find (member.value);
	check (m_itr == m_t.end(), "Account is already a member: " + member.to_string());
	m_t.emplace (get_self(), [&](auto &m) {
		m.member = member;
	});
}

void dao::removemember (const name& member) {
	require_auth (get_self());
	member_table m_t (get_self(), get_self().value);
	auto m_itr = m_t.find (member.value);
	check (m_itr != m_t.end(), "Account is not a member: " + member.to_string());
	m_t.erase (m_itr);
}

void dao::eraseobjs (const name& scope) {
	require_auth (get_self());
	object_table o_t (get_self(), scope.value);
	auto o_itr = o_t.begin();
	while (o_itr != o_t.end()) {
		o_itr = o_t.erase (o_itr);
	}
}

void dao::togglepause () {
	require_auth (get_self());
	config_table      config_s (get_self(), get_self().value);
   	Config c = config_s.get_or_create (get_self(), Config());   
	if (c.ints.find ("paused") == c.ints.end() || c.ints.at("paused") == 0) {
		c.ints["paused"]	= 1;
	} else {
		c.ints["paused"] 	= 0;
	} 	
	config_s.set (c, get_self());
}

void dao::remperiods (const uint64_t& begin_period_id, 
                           const uint64_t& end_period_id) {
	require_auth (get_self());
    bank.remove_periods (begin_period_id, end_period_id);
}

void dao::resetperiods () {
	require_auth (get_self());
	bank.reset_periods();
}

void dao::setconfig (	const map<string, name> 		names,
						const map<string, string>       strings,
						const map<string, asset>        assets,
						const map<string, time_point>   time_points,
						const map<string, uint64_t>     ints,
						const map<string, float>        floats,
						const map<string, transaction>  trxs)
{
	require_auth (get_self());

	config_table      config_s (get_self(), get_self().value);
   	Config c = config_s.get_or_create (get_self(), Config());   

	// retain last_ballot_id from the current configuration if it is not provided in the new one
	name last_ballot_id	;
	if (names.find("last_ballot_id") != names.end()) { 
		last_ballot_id	= names.at("last_ballot_id"); 
	} else if (c.names.find("last_ballot_id") != c.names.end()) {
		last_ballot_id	= c.names.at("last_ballot_id");
	}

	c.names						= names;
	c.names["last_ballot_id"] 	= last_ballot_id;

	c.strings		= strings;
	c.assets		= assets;
	c.time_points	= time_points;
	c.ints			= ints;
	c.floats		= floats;
	c.trxs			= trxs;

	config_s.set (c, get_self());

	// validate for required configurations
    string required_names[]{ "reward_token_contract", "telos_decide_contract", "last_ballot_id"};
    for (int i{ 0 }; i < std::size(required_names); i++) {
		check (c.names.find(required_names[i]) != c.names.end(), "name configuration: " + required_names[i] + " is required but not provided.");
	}
}

void dao::updversion (const string& component, const string& version) {
	config_table      config_s (get_self(), get_self().value);
   	Config c = config_s.get_or_create (get_self(), Config());   
	c.strings[component] = version;
	config_s.set (c, get_self());
}

void dao::setlastballt ( const name& last_ballot_id) {
	require_auth (get_self());
	config_table      config_s (get_self(), get_self().value);
   	Config c = config_s.get_or_create (get_self(), Config());   
	c.names["last_ballot_id"]			=	last_ballot_id;
	config_s.set (c, get_self());
}

void dao::enroll (	const name& enroller,
					const name& applicant, 
					const string& content) {

	check ( !is_paused(), "Contract is paused for maintenance. Please try again later.");	

	// this action is linked to the daomain@enrollers permission
	applicant_table a_t (get_self(), get_self().value);
	auto a_itr = a_t.find (applicant.value);
	check (a_itr != a_t.end(), "Applicant not found: " + applicant.to_string());

	config_table      config_s (get_self(), get_self().value);
   	Config c = config_s.get_or_create (get_self(), Config());  

	asset one_vote = asset { 100, common::S_VOTE };
	string memo { "Welcome to the DAO!"};
	action(	
		permission_level{get_self(), "active"_n}, 
		c.names.at("telos_decide_contract"), "mint"_n, 
		make_tuple(applicant, one_vote, memo))
	.send();

	// Should we also send 1 REWARD?  I think so, so I'll put it for now, but comment it out
	asset one_reward = asset { 1, common::S_REWARD };
	bank.makepayment (-1, applicant, one_reward, memo, common::NO_ASSIGNMENT, 1);

	member_table m_t (get_self(), get_self().value);
	auto m_itr = m_t.find (applicant.value);
	check (m_itr == m_t.end(), "Account is already a member: " + applicant.to_string());
	m_t.emplace (get_self(), [&](auto &m) {
		m.member = applicant;
	});

	a_t.erase (a_itr);
}	

void dao::remapply (const name& applicant) {
	require_auth (get_self());
	applicant_table a_t (get_self(), get_self().value);
	auto a_itr = a_t.find (applicant.value);
	a_t.erase (a_itr);
}

void dao::debugmsg (const string& message) {
	require_auth (get_self());
	debug (message);
}

void dao::apply (const name& applicant, 
						const string& content) {

	check ( !is_paused(), "Contract is paused for maintenance. Please try again later.");	
	require_auth (applicant);

	member_table m_t (get_self(), get_self().value);
	auto m_itr = m_t.find (applicant.value);
	check (m_itr == m_t.end(), "Applicant is already a member: " + applicant.to_string());

	applicant_table a_t (get_self(), get_self().value);
	auto a_itr = a_t.find (applicant.value);

	if (a_itr != a_t.end()) {
		a_t.modify (a_itr, get_self(), [&](auto &a) {
			a.content = content;
			a.updated_date = current_time_point();
		});
	} else {
		a_t.emplace (get_self(), [&](auto &a) {
			a.applicant = applicant;
			a.content = content;
		});
	}
}				

name dao::register_ballot (const name& proposer, 
							const map<string, string>& strings) 
{
	check (has_auth (proposer) || has_auth(get_self()), "Authentication failed. Must have authority from proposer: " +
		proposer.to_string() + "@active or " + get_self().to_string() + "@active.");
	
	qualify_proposer(proposer);

	config_table      config_s (get_self(), get_self().value);
   	Config c = config_s.get_or_create (get_self(), Config());  
	
	// increment the ballot_id
	name new_ballot_id = name (c.names.at("last_ballot_id").value + 1);
	c.names["last_ballot_id"] = new_ballot_id;
	config_s.set(c, get_self());
	
	decidespace::decide::ballots_table b_t (c.names.at("telos_decide_contract"), c.names.at("telos_decide_contract").value);
	auto b_itr = b_t.find (new_ballot_id.value);
	check (b_itr == b_t.end(), "ballot_id: " + new_ballot_id.to_string() + " has already been used.");

	vector<name> options;
   	options.push_back ("pass"_n);
   	options.push_back ("fail"_n);

	action (
      permission_level{get_self(), "active"_n},
      c.names.at("telos_decide_contract"), "newballot"_n,
      std::make_tuple(
			new_ballot_id, 
			"poll"_n, 
			get_self(), 
			common::S_VOTE, 
			"1token1vote"_n, 
			options))
   .send();

	//	  // default is to DAO all tokens, not just staked tokens
	//    action (
	//       permission_level{get_self(), "active"_n},
	//       c.telos_decide_contract, "togglebal"_n,
	//       std::make_tuple(new_ballot_id, "votestake"_n))
	//    .send();

   action (
	   	permission_level{get_self(), "active"_n},
		c.names.at("telos_decide_contract"), "editdetails"_n,
		std::make_tuple(
			new_ballot_id, 
			strings.at("title"), 
			strings.at("description"),
			strings.at("content")))
   .send();

   auto expiration = time_point_sec(current_time_point()) + c.ints.at("voting_duration_sec");
   
   action (
      permission_level{get_self(), "active"_n},
      c.names.at("telos_decide_contract"), "openvoting"_n,
      std::make_tuple(new_ballot_id, expiration))
   .send();

	return new_ballot_id;
}

void dao::create (const name&						scope,
					const map<string, name> 		names,
					const map<string, string>       strings,
					const map<string, asset>        assets,
					const map<string, time_point>   time_points,
					const map<string, uint64_t>     ints,
					const map<string, float>        floats,
					const map<string, transaction>  trxs)
{
	check ( !is_paused(), "Contract is paused for maintenance. Please try again later.");	
	const name owner = names.at("owner");

	check (has_auth (owner) || has_auth(get_self()), "Authentication failed. Must have authority from owner: " +
		owner.to_string() + "@active or " + get_self().to_string() + "@active.");
	
	qualify_proposer (owner);

	object_table o_t (get_self(), scope.value);
	o_t.emplace (get_self(), [&](auto &o) {
		o.id                       	= o_t.available_primary_key();
		o.names                    	= names;
		o.strings                  	= strings;
		o.assets                  	= assets;
		o.time_points              	= time_points;
		o.ints                     	= ints;
		o.floats                   	= floats;
		o.trxs                     	= trxs;

		config_table      config_s (get_self(), get_self().value);
   		Config c = config_s.get_or_create (get_self(), Config()); 
		o.strings["client_version"] = get_string(c.strings, "client_version");
		o.strings["contract_version"] = get_string(c.strings, "contract_version");

		if (scope == "proposal"_n) {
			name proposal_type	= names.at("type");
			o.names["ballot_id"]		= register_ballot (owner, strings);

			/* default trx_action_account to dao */
			if (names.find("trx_action_contract") == names.end()) {
				o.names["trx_action_contract"] = get_self();
			}

			name action_on_approval = name ("passprop");  // default action is 'passprop'
			if (names.find("trx_action_name") != names.end()) {
				action_on_approval = names.at("trx_action_name");
			}

			// this transaction executes if the proposal passes
			transaction trx (time_point_sec(current_time_point())+ (60 * 60 * 24 * 35));
			trx.actions.emplace_back(
				permission_level{get_self(), "active"_n}, 
				o.names.at("trx_action_contract"), action_on_approval, 
				std::make_tuple(o.id));
			trx.delay_sec = 0;
			o.trxs["exec_on_approval"]      = trx;      
		}
	});      
}

void dao::clrdebugs (const uint64_t& starting_id, const uint64_t& batch_size) {
	check (has_auth ("gba"_n) || has_auth(get_self()), "Requires higher permission.");
	debug_table d_t (get_self(), get_self().value);
	auto d_itr = d_t.find (starting_id);

	while (d_itr->debug_id <= starting_id + batch_size) {
		d_itr = d_t.erase (d_itr);
	}

	eosio::transaction out{};
	out.actions.emplace_back(permission_level{get_self(), "active"_n}, 
							get_self(), "clrdebugs"_n, 
							std::make_tuple(d_itr->debug_id, batch_size));
	out.delay_sec = 1;
	out.send(get_next_sender_id(), get_self());    
}

void dao::addperiod (const time_point& start_date, const time_point& end_date) {
	require_auth (get_self());
	bank.addperiod (start_date, end_date);
}

void dao::compchalleng (const name& completer, const uint64_t& challenge_id) 
{
	check(!is_paused(), "Contract is paused for maintenance. Please try again later.");
	require_auth(completer);

	object_table o_t_challenge(get_self(), "challenge"_n.value);
	auto c_itr = o_t_challenge.find(challenge_id);
	check(c_itr != o_t_challenge.end(), "Challenge does not exist: " + std::to_string(challenge_id));

	// check in the completer's list of objects to determine if they have completed this challenge already
	// TODO: what if a challenge is erased and a second one is created with the same ID
	member_table m_t (get_self(), get_self().value);
	auto m_itr = m_t.find (completer.value);
	check (m_itr != m_t.end(), "Challenge completer is not a member: " + completer.to_string());
	for (chg_id : m_itr->completed_challenges) {
		check (challenge_id != chg_id, "Member: ", completer.to_string(), " has already completed challenge id: " + std::to_string(challenge_id));)
	}
	m_t.modify (m_itr, get_self(), [&](auto &m) {
		m.completed_challenges.push_back(challenge_id);
	});

	string memo{"One time reward for Hypha Challenge. Challenge Name ID: " + std::to_string(challenge_id)};
	bank.makepayment(-1, completer, o_itr->assets.at("reward_amount"), memo, challenge_id, 1);
	bank.makepayment(-1, completer, o_itr->assets.at("usd_amount"), memo, challenge_id, 1);
	bank.makepayment(-1, completer, o_itr->assets.at("vote_amount"), memo, challenge_id, 1);
}

void dao::closeprop(const uint64_t& proposal_id) {

	check ( !is_paused(), "Contract is paused for maintenance. Please try again later.");	

	object_table o_t (get_self(), "proposal"_n.value);
	auto o_itr = o_t.find(proposal_id);
	check (o_itr != o_t.end(), "Scope: " + "proposal"_n.to_string() + "; Object ID: " + std::to_string(proposal_id) + " does not exist.");
	auto prop = *o_itr;

	config_table      config_s (get_self(), get_self().value);
   	Config c = config_s.get_or_create (get_self(), Config());  

	decidespace::decide::ballots_table b_t (c.names.at("telos_decide_contract"), c.names.at("telos_decide_contract").value);
	auto b_itr = b_t.find (prop.names.at("ballot_id").value);
	check (b_itr != b_t.end(), "ballot_id: " + prop.names.at("ballot_id").to_string() + " not found.");

	decidespace::decide::treasuries_table t_t (c.names.at("telos_decide_contract"), c.names.at("telos_decide_contract").value);
	auto t_itr = t_t.find (common::S_VOTE.code().raw());
	check (t_itr != t_t.end(), "Treasury: " + common::S_VOTE.code().to_string() + " not found.");

	asset quorum_threshold = adjust_asset(t_itr->supply, 0.20000000);  // quorum of 20%
	map<name, asset> votes = b_itr->options;
	asset votes_pass = votes.at("pass"_n);
	asset votes_fail = votes.at("fail"_n);

	string debug_str = " Total Vote Weight: " + b_itr->total_raw_weight.to_string() + "\n";
	debug_str = debug_str + " Total Supply: " + t_itr->supply.to_string() + "\n"; 
	debug_str = debug_str + " Quorum Threshold: " + quorum_threshold.to_string() + "\n";	
	debug_str = debug_str + " Votes Passing: " + votes_pass.to_string() + "\n";
	debug_str = debug_str + " Votes Failing: " + votes_fail.to_string() + "\n";

	bool passed = false;
	if (b_itr->total_raw_weight >= quorum_threshold && 			// must meet quorum
		votes_pass > votes_fail) {  // must have 50% of the vote power
		debug_str = debug_str + "Proposal passed. Executing transaction. ";
		passed = true;
		prop.trxs.at("exec_on_approval").send(current_block_time().to_time_point().sec_since_epoch(), get_self());		
	} else {
		change_scope ("proposal"_n, proposal_id, "failedprops"_n, false);
		change_scope ("proposal"_n, proposal_id, "proparchive"_n, true);
	}

	debug_str = debug_str + string ("Ballot ID read from prop for closing ballot: " + prop.names.at("ballot_id").to_string() + "\n");
	action (
		permission_level{get_self(), "active"_n},
		c.names.at("telos_decide_contract"), "closevoting"_n,
		std::make_tuple(prop.names.at("ballot_id"), true))
	.send();

	debug (debug_str);
}

void dao::passprop (const uint64_t& proposal_id) {
	require_auth (get_self());

	object_table o_t(get_self(), "proposal"_n.value);
	auto o_itr = o_t.find(proposal_id);
	check (o_itr != o_t.end(), "Proposal ID does not exist: " + std::to_string(proposal_id));
	check (o_itr->names.find("type") != o_itr->names.end(), "Proposal object does not have a type to promote object to. proposal_id: " + std::to_string(proposal_id));

	vector<name> new_scopes = {o_itr->names.at("type"), name("proparchive")};
	change_scope("proposal"_n, proposal_id, new_scopes, true);
}


void dao::qualify_proposer (const name& proposer) {
	// Should we require that users hold Hypha before they are allowed to propose?  Disabled for now.
	// check (bank.holds_hypha (proposer), "Proposer: " + proposer.to_string() + " does not hold REWARD.");
}
