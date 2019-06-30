#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <string>

using namespace eosio;
using namespace std;

CONTRACT opinionbetha : public contract {
  public:
    using contract::contract;
    opinionbetha(eosio::name receiver, eosio::name code, datastream<const char*> ds):
    contract(receiver, code, ds),
    _polls(receiver, code.value),
    _options(receiver, code.value),
    _users(receiver, code.value),
    _courts(receiver, code.value)
    {}

    ACTION addpoll(uint64_t pollId, string pollName, uint64_t funds, uint64_t ownerId);
    ACTION uservote(uint64_t pollId, uint64_t userId, uint64_t optionId);
    ACTION judgevote(uint64_t pollId, uint64_t userId, uint64_t optionId);
    ACTION addjudge(uint64_t pollId, uint64_t userId);
    ACTION addcourt(uint64_t pollId, uint64_t maxJudges, uint64_t funds);
    ACTION toforecast(uint64_t pollId);
    ACTION setlimit(uint64_t pollId, uint64_t maxUsers);
    ACTION setfinished(uint64_t pollId);
    ACTION resolve(uint64_t courtId);

    ACTION mintost(uint64_t userId, uint64_t value);
    ACTION illegalmint(uint64_t userId, uint64_t value);
        
    [[eosio::action]]
    bool transfer(uint64_t from, uint64_t to, uint64_t value);
    ACTION multi(vector<uint64_t> to_list, uint64_t foreach);
  
    
    [[eosio::action]]
    bool safetransfer(uint64_t userId, uint64_t value);
    
    ACTION deletejudge(uint64_t pollId, uint64_t userId);
    ACTION userauth(uint64_t userId);
    ACTION attachoption(uint64_t pollId, uint64_t optionId);
    
    ACTION refund(uint64_t userId);
    
    
  private:
    TABLE Poll {
      uint64_t  pollId;
      string    pollName;
      string    eventType;
      uint64_t  ownerId;
      uint64_t  funds;
      uint64_t  totalVotes;
      uint64_t  maxUsers;
      bool      isFinished;
      
      auto primary_key() const { return pollId; }
    };
    typedef multi_index<"polls"_n, Poll> pollstable;
    
    TABLE Court {
      uint64_t pollId;
      uint64_t funds;
      uint64_t numJudges;
      uint64_t maxJudges;
      uint64_t verdict;
      uint64_t totalVerdicts;
      bool     resolved;
      
      auto primary_key() const { return pollId; }
    };
    typedef multi_index<"courts"_n, Court> courtstable;
    
    TABLE Option {
      uint64_t          optionId;
      uint64_t          pollId;
      vector<uint64_t>  voters;
      vector<uint64_t>  judges;
      
      auto primary_key() const { return optionId; }
    };
    typedef multi_index<"options"_n, Option> optionstable;

    TABLE User {
      uint64_t          userId;
      uint64_t          balance;
      uint64_t          frozen;
      vector<uint64_t>  voted;
      vector<uint64_t>  courtlist;
      
      auto primary_key() const { return userId; }
    };
    typedef multi_index<"users"_n, User> userstable;
    
    template <class T>
    bool contains(vector<T> v, T value) {
      if (find(v.begin(), v.end(), value) != v.end())
        return true;
      return false;
    }
    
    bool hasvoted(uint64_t pollId, uint64_t userId);
    
    int CONSENSUS_REQUIRED_RATE = 95;
    
    

// MAINTAINANCE ACTIONS
  
  userstable    _users;  
  pollstable    _polls;
  optionstable  _options;
  courtstable   _courts;
  
};

EOSIO_DISPATCH(opinionbetha, (refund)(deletejudge)(illegalmint)(resolve)(addpoll)(uservote)(mintost)(judgevote)(addjudge)(addcourt)(toforecast)(setlimit)(setfinished))
