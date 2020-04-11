/*

Various tools for working with the DAO

git clone git@github.com:dappdever/dao-contracts.git
cd dao-contracts/scripts
yarn
node dao.js -f payloads/role-proposal.json -a -c -p && node dao.js -f payloads/assignment-proposal.json -a -c -p && node dao.js -f payloads/contrib-proposal.json -a -c -p
node dao.js -f payloads/config.json --config -h https://test.telos.kitchen
++++++++++++++++++++++++++++++++++++++++++++++++++++++ 
*/

const commandLineArgs = require("command-line-args");
const csv = require("fast-csv");
const fs = require('fs');

const { Api, JsonRpc } = require("eosjs");
const { JsSignatureProvider } = require("eosjs/dist/eosjs-jssig");
const fetch = require("node-fetch");
const { TextEncoder, TextDecoder } = require("util");

async function sendtrx (prod, host, contract, action, authorizer, data) {
  const rpc = new JsonRpc(host, { fetch });
  var defaultPrivateKey;
  if (prod) { defaultPrivateKey = process.env.PRIVATE_KEY; }
  else defaultPrivateKey = "5JsZRwcBkRyetarreHpLuVyBJwCimf1KxyLtbCpDtMB3Jz7mtt7";
  const signatureProvider = new JsSignatureProvider([defaultPrivateKey]);
  const api = new Api({rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder()});
  const actions = [{account: contract,name: action,authorization: [{actor: authorizer, permission: "active"}],data: data}];
  const result = await api.transact({actions: actions}, {blocksBehind: 3, expireSeconds: 30});
  console.log("Transaction Successfull : ", result.transaction_id);
}

async function closeProposal (prod, host, contract, proposal) {
  try {
    await sendtrx(prod, host, contract, "closeprop", contract,
      { "proposal_id": proposal.id}
    )
  } catch (e) {
    console.log ("------- BEGIN: Cannot close proposal ---------");
    if (e.json) {
      console.log (JSON.stringify (e.json.error.details[0].message))
    } else {
      console.log (JSON.stringify (e, null, 2))
    }
    await printProposal (proposal);
    console.log ("------- END: Cannot close proposal ---------");
    console.log ("\n\n\n")
  }
}

async function getVotingPeriod (host, contract) {
  let rpc;
  let options = {};

  rpc = new JsonRpc(host, { fetch });
  options.code = contract;
  options.scope = contract; 

  options.json = true;
  options.table = "config";

  const result = await rpc.get_table_rows(options);
  if (result.rows.length > 0) {
    return result.rows[0].ints.find(o => o.key === 'voting_duration_sec').value;
  } else {
    console.log ("ERROR:: Configuration has not been set.");
  }
}

async function getProposals (host, contract) {
  let rpc;
  let options = {};

  rpc = new JsonRpc(host, { fetch });
  options.code = contract;
  options.json = true;
  options.scope = "proposal";
  options.table = "objects";
  options.reverse = false;
  options.limit = 100;
  
  const result = await rpc.get_table_rows(options);
  if (result.rows.length > 0) {
    return result.rows;
  } 
  
  console.log ("There are no proposals of type");
  return undefined;  
}

async function getProposal (host, contract, proposal_id) {
  let rpc;
  let options = {};

  rpc = new JsonRpc(host, { fetch });
  options.code = contract;
  options.json = true;
  options.scope = "proposal";
  options.table = "objects";
  options.upper_bound = proposal_id;
  options.lower_bound = proposal_id;
  options.limit = 1;
  
  const result = await rpc.get_table_rows(options);
  if (result.rows.length > 0) {
    return result.rows[0];
  } 
  
  console.log ("There are no proposal with id of: ", proposal_id);
  return undefined;  
}

async function getLastCreatedProposal (host, contract) {
  let rpc;
  let options = {};

  rpc = new JsonRpc(host, { fetch });
  options.code = contract;
  options.scope = contract; 

  options.json = true;
  options.scope = "proposal";
  options.table = "objects";
  options.index_position = 2; // index #2 is "bycreated"
  options.key_type = 'i64';
  options.reverse = true;
  options.limit = 1;
  
  const result = await rpc.get_table_rows(options);
  if (result.rows.length > 0) {
    return result.rows[0];
  } 
  
  console.log ("There are no proposals of type");
  return undefined;  
}

const addPeriod = async (prod, host, contract, periods) => {

//async function addPeriods (prod, host, contract, periods) {
  for (let i = 0; i < periods.length; i++) {
    try {
      const { startdate, enddate } = periods[i]
      console.log ("Adding a period: ", startdate)
      await sendtrx (prod, host, contract, "addperiod", contract, {
        start_time: startdate,
        end_time: enddate
      });

      console.log("Successfully created phase: ", startdate)
    } catch (e) {
      console.error(e)
      console.log('Please, fix an error and run script again')
      process.exit (1);
    }
  }
}

const addPeriods = (file, prod, host, contract) => {
  let periods = []
  const handler = csv.parse({ headers: true })
  handler.on('data', row => periods.push(row))
  handler.on('end', () => addPeriod(prod, host, contract, periods))
  fs.createReadStream(file).pipe(handler)
}

class FileDetails {
  constructor (filename) {
    this.filename = filename
    this.exists = fs.existsSync(filename)
  }
}

async function loadOptions() {
  const optionDefinitions = [
    { name: "host", alias: "h", type: String, defaultValue: "https://test.telos.kitchen" },
    { name: "contract", type: String, defaultValue: "mygenericdao" },
    { name: "periods", type: Boolean, defaultValue: false },
    // extra safeguard, must also set key, host, and contract to prod
    { name: "prod", type: Boolean, defaultValue: false},  

    // file to read from if required
    { name: "file", alias: "f", type: filename => new FileDetails(filename)  },

    // these are typically used together as -p -a -c to open a proposal, 
    // vote for it, wait until the voting period expries, and 
    // then close the proposal
    { name: "propose", alias: "p", type: Boolean, defaultValue: false },
    { name: "approve", alias: "a", type: Boolean, defaultValue: false },
    { name: "close", alias: "c", type: Boolean, defaultValue: false },

    // close a specific proposal id    
    { name: "closepropid", type: String},

    // loop through all open proposals and try to close them
    // does not halt on first error, reports error if close not possible
    { name: "closeall", type: Boolean, defaultValue: false},

    // set a config
    { name: "config", type: Boolean, defaultValue: false },    
    { name: "print_proposal", alias: "x", type: String}
    // see here to add new options:
    //   - https://github.com/75lb/command-line-args/blob/master/doc/option-definition.md
  ];

  return commandLineArgs(optionDefinitions);
}

function sleep(ms, msg){
  console.log ( "\nPlease wait : ", ms, " ms ... ", msg, "\n");
  return new Promise(resolve=>{
    setTimeout(resolve,ms)
  })
}

async function getValue (map, key) {
  try {
    return map.find(o => o.key === key).value
  }
  catch (e) {
    return "Not Found"
  }
}

async function printProposal (p) {
  console.log (JSON.stringify(p, null, 2))
  // console.log("Proposer       : ", await getValue(p.names, 'proposer'))
  // console.log("Owner          : ", await getValue(p.names, 'owner'))
  // console.log("Title          : ", await getValue(p.strings, 'title'))
  // console.log("Proposal ID    : ", p.id)
  // console.log("Proposal Type  : ", await getValue(p.names, 'proposal_type'))
  // console.log("Type           : ", await getValue(p.names, 'type'))
  // console.log("Annual USD     : ", await getValue(p.assets, "annual_usd_salary"))
  // console.log("REWARD Amount   : ", await getValue(p.assets, "reward"))
  // console.log("VOTE Amount  : ", await getValue(p.assets, "VOTE_amount"))
  // console.log("Created Date   : ", p.created_date)
  // console.log("Description    : ", await getValue(p.strings, "description"))
  // console.log("Content        : \n", await getValue(p.strings, "content"))
  console.log("")
}


const main = async () => {
  const opts = await loadOptions();

  // setting configuration
  if (opts.file && opts.config) {
    const config = JSON.parse(fs.readFileSync(opts.file.filename, 'utf8'));
    console.log ("\nParsing the configuration from : ", opts.file.filename);
    console.log ("-- reward_token_contract   : ", config.data.names.find(o => o.key === 'reward_token_contract').value);
    console.log ("-- telos_decide_contract  : ", config.data.names.find(o => o.key === 'telos_decide_contract').value);

    console.log ("\nSubmitting configuration : ", opts.file.filename);
    await sendtrx(opts.prod, opts.host, opts.contract, "setconfig", opts.contract, config.data);
  }

  // proposing
  else if (opts.file && opts.propose) {

    const proposal = JSON.parse(fs.readFileSync(opts.file.filename, 'utf8'));
    console.log ("\nParsing the proposal from : ", opts.file.filename);
    console.log ("-- title            : ", proposal.data.strings.find(o => o.key === 'title').value);
    console.log ("-- type             : ", proposal.data.names.find(o => o.key === 'type').value);
    console.log ("-- proposer         : ", proposal.data.names.find(o => o.key === 'owner').value);

    console.log ("\nSubmitting proposal : ", opts.file.filename);
    await sendtrx(opts.prod, opts.host, opts.contract, "create", 
      proposal.data.names.find(o => o.key === 'owner').value, 
      proposal.data);

    if (opts.approve || opts.close) {

      // sleep to ensure the table is written
      await sleep (10000, "Eliminating likelihood of REVERT before retrieving proposal...");
      const lastProposal = await getLastCreatedProposal(opts.host, opts.contract);

      if (opts.approve) {

        // vote for the proposal
        var options = [];
        options.push ("pass");
  
        console.log ("\Approving the proposal");
        console.log ("-- calling trailservice::castvote with the following parms:");
        console.log ("-- -- voter       : ", proposal.data.names.find(o => o.key === 'owner').value);
        console.log ("-- -- ballot_id   : ", lastProposal.names.find(o => o.key === 'ballot_id').value);
        console.log ("-- -- options     : ", options);
  
        await sendtrx(opts.prod, opts.host, "telos.decide", "castvote", 
          proposal.data.names.find(o => o.key === 'owner').value, 
          { "voter":proposal.data.names.find(o => o.key === 'owner').value, 
            "ballot_name":lastProposal.names.find(o => o.key === 'ballot_id').value, 
            "options":options });
      }
  
      if (opts.close) {
        
        console.log ("\nClosing the proposal (after the wait)");
        console.log ("-- calling closeprop with the following parms:");
        console.log ("-- -- proposal_id   : ", lastProposal.id);

        // sleep another 1 hour to ensure the ballot open window is complete
        const sleepDuration = (await getVotingPeriod(opts.host, opts.contract) * 1000) + 20000;
        await sleep (sleepDuration, "Waiting " + sleepDuration + " seconds while the ballot expiration expires...");
   
        // close the DAproposal
        await sendtrx(opts.prod, opts.host, opts.contract, "closeprop", 
          proposal.data.names.find(o => o.key === 'owner').value, 
          { "proposal_id":lastProposal.id });
      }
    }
  } else if (opts.closeall) {

      let proposals = await getProposals(opts.host, opts.contract)
      proposals.forEach (async function (proposal) {
        await closeProposal (opts.prod, opts.host, opts.contract, proposal);
      });      
  } else if (opts.closepropid) {
    await closeProposal (opts.prod, opts.host, opts.contract, await getProposal (opts.host, opts.closepropid))
  } else if (opts.print_proposal) {
    await printProposal (await getProposal(opts.host, opts.contract, opts.print_proposal))
  } else if (opts.file && opts.periods) {
      // const proposal = JSON.parse(fs.readFileSync(opts.file.filename, 'utf8'));
      //    const loadPeriods = () => {
      addPeriods(opts.file.filename, opts.prod, opts.host, opts.contract)
    } else {
    console.log ("Command unsupported. You used: ", JSON.stringify(opts, null, 2));
  }  
}

main()
