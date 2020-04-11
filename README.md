## Quick Start
```
git clone git@github.com:dappdever/dao-contracts.git
cd dao-contracts/scripts
yarn
node dao.js -f payloads/contrib-proposal.json -p -a -c  
```

## Walkthrough of Role and Assignment 
Take a look at the sample proposals in scripts/payloads.

To use the dao.js CLI to make a proposal, vote on the proposal, and close the proposal to perform the transfer, run this:

```
node dao.js -f payloads/contrib-proposal.json -p -a -c  
```

After that, you can run the role and assignment proposals:
```
node dao.js -f payloads/role-proposal.json -p -a -c  
```

When that is successful, run this to find the role ID of the role you just created:
```
cleos -u https://test.telos.kitchen get table -r -l 1 mygenericdao role objects | jq ".rows[0].id"
```

Use the integer that is printed and put it in the "data.ints.role_id" field in the payloads/assignment-proposal.json file.  This tells the DAO that you are applying for an assignment to do the role that you just created. While you're looking at the assignment proposal file, note the attribute for ```data.ints.time_share_x100```.  This represents the % of their time, given a 40 hour work week, that this proposer intends to work in this role.  Then run the assignment proposal:
```
node dao.js -f payloads/assignment-proposal.json -p -a -c 
```

Now, check that you created an assignment. 
```
cleos -u https://test.telos.kitchen get table -r -l 1 mygenericdao role objects
```

After the next Monday at 00:00:000, the period will increment and you'll be able to claim your salary. Of course, it will be abbreviated pay period based on when the assignment was created.

### Contribution Proposal
The contrib-proposal.json is below.  Here's an overview of the fields:
- owner: accounting creating this object, which is not always the proposer
- proposer: account proposing this proposal
- receipient: account that will receive the payout
- trx_action_name: this tells the DAO which action to call if this proposal passes
- type: proposal type (this is perhaps duplicative to the action name)
- description: text
- title: text
- reward_amount: amount of REWARD to be paid (note that ```mygenericdao``` must be the issuer for this token)
- vote_amount: amount of VOTEPOW to be paid via the ```telos.decide``` treasury

The user can propose any number of variables throughout this JSON and it will be saved with the proposal and may be rendered in any UI. If we were putting a Ricardian contract with this, it should assert that "I certify that all data contained in the parameters is accurate."

```
{
  "data": {
    "scope": "proposal",
    "names": [
      {
        "key": "owner",
        "value": "buckyjohnson"
      },
      {
        "key": "proposer",
        "value": "buckyjohnson"
      },
      {
        "key": "recipient",
        "value": "buckyjohnson"
      },
      {
        "key": "trx_action_name",
        "value": "makepayout"
      },
      {
        "key": "type",
        "value": "payout"
      }
    ],
    "strings": [
      {
        "key": "description",
        "value": "Payout for reimbursement for a Meetup"
      },
      {
        "key": "title",
        "value": "I led a local meetup to spread the word on the movement"
      }
    ],
    "assets": [
      {
        "key": "reward_amount",
        "value": "1250.00 REWARD"
      },
      {
        "key": "vote_amount",
        "value": "821.00 VOTEPOW"
      }
    ],
    "time_points": [
      {
        "key": "contribution_date",
        "value": "2020-01-30T13:00:00.000"
      }
    ],
    "ints": [],
    "floats": [],
    "trxs": []
  }
}
```