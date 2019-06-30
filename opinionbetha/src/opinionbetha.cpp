#include <opinionbetha.hpp>

//  OPERATIONAL ACTIONS
ACTION opinionbetha::addpoll(
  uint64_t pollId, 
  string pollName, 
  uint64_t funds, 
  uint64_t ownerId
  ) {
  auto poll = _polls.find(pollId);
  
  if(!transfer(ownerId, 0, funds)) 
    return;
  
  if (poll == _polls.end()) {
    _polls.emplace(get_self(), [&](auto& p) {
      p.pollId = pollId;
      p.pollName = pollName;
      p.funds = funds;
      p.ownerId = ownerId;
      p.totalVotes = 0;
      p.maxUsers = 9999;
      p.eventType = "Poll";
      p.isFinished = false;
    });
  } else {
    _polls.modify(poll, get_self(), [&](auto& p) {
      p.pollId = pollId;
      p.pollName = pollName;
      p.funds = funds;
      p.ownerId = ownerId;
      p.totalVotes = 0;
      p.maxUsers = 9999;
      p.eventType = "Poll";
      p.isFinished = false;
    }); 
  }
  print("| Poll_", pollId, " created: funds_", funds, "_OST |");
}

ACTION opinionbetha::addcourt(uint64_t pollId, uint64_t maxJudges, uint64_t funds) {
  auto& poll = _polls.get(pollId, "Poll not found");
  
  if (poll.eventType != "Forecast" || !poll.isFinished) {
    print("! eventType exception: finished Forecasts only !");
    return;
  }
  
  auto court = _courts.find(pollId);
  
  if(court == _courts.end()) {
    _courts.emplace(get_self(), [&](auto& p) {
      p.pollId = pollId;
      p.maxJudges = maxJudges;
      p.funds = funds;
    });
    _polls.modify(poll, get_self(), [&](auto& p) {
      p.eventType = "Forecast on court";
    });
    if (!transfer(poll.ownerId, 0, funds))
      return;
  } else {
    print("! data exception: Court exists already !");
  }

}

ACTION opinionbetha::uservote(uint64_t pollId, uint64_t userId, uint64_t optionId) {
   
  if (hasvoted(pollId, userId)) {
    print("! Double vote !");
    return;
  }
  
  auto poll = _polls.find(pollId);
  auto user = _users.find(userId);

  if (poll == _polls.end()) {
    print("! data exception: No such poll exists !");
    return;
  }
    
  attachoption(pollId, optionId);
  
  auto option = _options.find(optionId);
  
  _options.modify(option, get_self(), [&](auto& p) {
    p.voters.push_back(userId);
  });
  
  _polls.modify(poll, get_self(), [&](auto& p) {
    p.totalVotes = p.totalVotes + 1;
  });
  
  _users.modify(user, get_self(), [&](auto& p) {
    p.voted.push_back(pollId);
  });
  
  if (poll->eventType == "Limited poll") {
    if (!safetransfer(userId, poll->funds / poll->maxUsers)) {
      return;
    }
    print (" (Fixed payout in limited poll_", pollId, ") ");
  }
  
  print (" (user_", userId, " vote : option_", optionId, "; poll_", pollId, ") ");
}

ACTION opinionbetha::addjudge(uint64_t pollId, uint64_t userId) {
  auto& court = _courts.get(pollId, "Court not found.");
  
  if (court.numJudges == court.maxJudges) {
    print(" (Max cap of judges reached) ");
    return;
  }
  
    userauth(userId);
    auto user = _users.find(userId);
    auto j = user->courtlist;
    if (contains(j, pollId)) {
      print (" (data exception: user_", userId, " is judge already) ");
      return;
    }
    _users.modify(user, get_self(), [&](auto& p) {
      p.courtlist.push_back(pollId);
    });
    _courts.modify(court, get_self(), [&](auto& p) {
      p.numJudges += 1;
    });
    print(" (user_", userId, " added to court _", pollId, ") ");
  }

ACTION opinionbetha::judgevote(uint64_t pollId, uint64_t userId, uint64_t optionId) {
  userauth(userId);
  attachoption(pollId, optionId);
    auto c = _users.find(userId)->courtlist;
      if (find(c.begin(), c.end(), pollId) != c.end()) {
        _options.modify(_options.find(optionId), get_self(), [&](auto& p) {
          p.judges.push_back(userId);
      });
      
      deletejudge(pollId, userId);
       
      print (" (Judge vote from user_", userId, ") ");
      _courts.modify(_courts.find(pollId), get_self(), [&](auto& p) {
        p.totalVerdicts += 1;
      });
      if (_courts.find(pollId)->totalVerdicts == _courts.find(pollId)->maxJudges) {
        resolve(pollId);
        return;
      }
    return;
  }
  print("! data exception: User was not invited as judge: ", userId, "! ");
}

//  TRANSACTIONAL ACTIONS

ACTION opinionbetha::mintost(uint64_t userId, uint64_t value) {
  // if (_globals.mint_done) {
  //   print ("! mint exception: minting can be done only once !");
  //   return;
  // }
  userauth(userId);
  auto user = _users.find(userId);
  _users.modify(user, get_self(), [&](auto& p) {
    p.balance += value;
  });
  // _globals.mint_done = true;
}

[[eosio::action]]
bool opinionbetha::transfer(uint64_t from, uint64_t to, uint64_t value) {
  if (value == 0) {
    return false;
  }
  userauth(from);
  userauth(to);
  auto _from = _users.find(from);
  auto _to = _users.find(to);
  auto balance_from = _from->balance;
  auto balance_to = _to->balance;
  if (balance_from >= value && balance_to + value >= balance_to) {
    _users.modify(_from, get_self(), [&](auto& p) {
      p.balance -= value;
    });
    _users.modify(_to, get_self(), [&](auto& p) {
      p.balance += value;
    });
    if (to == 0) {
      _users.modify(_from, get_self(), [&](auto& p) {
        p.frozen += value;
      });
    }
    print(
      " (Successful transfer ", 
      value,
      " OST ", 
      from, 
      " -> ", 
      to,
      ") ");
    return true;
  }
  print("! transfer exception: insufficient funds or overflow [", from, " -> ", to, "] !");
  return false;
}

ACTION opinionbetha::multi(vector<uint64_t> to_list, uint64_t foreach) {
  for (uint64_t _to : to_list) {
    userauth(_to);
    if(!safetransfer(_to, foreach))
      return;
  }
  print(" (safe mutlitransfer done) ");
}

//  MAINTAINANCE ACTIONS

ACTION opinionbetha::deletejudge(uint64_t pollId, uint64_t userId) {
  userauth(userId);
  auto& user = _users.get(userId);
  auto c = user.courtlist;
  
  if (c.empty()) {
    print("! data exception: This person doesn`t judge anything !");
    return;
  }
    auto del = find(c.begin(), c.end(), pollId);
    for (int i = 0; i < c.size(); i++) {
      if (c[i] == pollId) {
        _users.modify(user, get_self(), [&](auto& p) {
         p.courtlist.erase(p.courtlist.begin() + i);
      });
        return;
      }
    }
    // if (del != c.end()) {
    //   _users.modify(user, get_self(), [&](auto& p) {
    //     p.courtlist.erase(del);
    //   });
    //   print("Judge_", userId, " removed from court_", pollId, " after vote");
    //   return;
    // }
    print("! data exception: This person doesn`t judge poll_", pollId, " !");
}

ACTION opinionbetha::toforecast(uint64_t pollId) {
  auto poll = _polls.find(pollId);
  if (poll != _polls.end()) {
    if (poll->isFinished) {
      print("! event exception: Poll is finished and can`t be changed !");
      return;
    }
    _polls.modify(poll, get_self(), [&](auto& p) {
        p.eventType = "Forecast";
        p.maxUsers = 9999;
    });
    return;
  }
}

ACTION opinionbetha::setlimit(uint64_t pollId, uint64_t maxUsers) {
  auto poll = _polls.find(pollId);
  if (poll != _polls.end() && poll->maxUsers == 9999) {
    if (poll->isFinished) {
      print("! event exception: Poll is finished and can`t be changed !");
      return;
    }
    _polls.modify(poll, get_self(), [&](auto& p) {
      p.maxUsers = maxUsers;
      p.eventType = "Limited Poll";
    });
    return;
  }
  print("! data exception: No such poll exists or the limit is set already !");
}

ACTION opinionbetha::setfinished(uint64_t pollId) {
  auto poll = _polls.find(pollId);
  if (poll != _polls.end()) {
    if (poll->isFinished) {
      print("! event exception: Event is finished already !");
      return;
    }
    if (poll->eventType == "Poll") {
      vector<uint64_t> participants;
      for (auto user : _users) {
        if (contains(user.voted, pollId))
          participants.push_back(user.userId);
      }
      multi(participants, poll->funds / participants.size());
      refund(poll->ownerId);
      return;
    }
    _polls.modify(poll, get_self(), [&](auto& p) {
      p.isFinished = true;
    });
  }
}

ACTION opinionbetha::resolve(uint64_t courtId) {
  auto court = _courts.find(courtId);
  auto poll = _polls.find(courtId);
  if (court != _courts.end()) {
    map<uint64_t, float> percents;
    uint64_t max = 0;
    uint64_t verdict;
    for (auto option : _options) {
      if (option.pollId == courtId) {
        percents[option.optionId] = option.judges.size() / court->maxJudges;
        if (percents[option.optionId] > max) {
          max = percents[option.optionId];
          verdict = option.optionId;
        }
      }
    }
    
    if (verdict >= CONSENSUS_REQUIRED_RATE) {
      _courts.modify(court, get_self(), [&](auto& p) {
        p.resolved = true;
        p.verdict = verdict;
      });
      multi(_options.find(verdict)->judges, court->funds / court-> maxJudges);
      
      if (_options.find(verdict)->voters.empty()) {
        print(" (None of voters guessed on forecast_", courtId , ") ");
        return;
      }
      
      multi(_options.find(verdict)->voters, poll->funds 
          / _options.find(verdict)->voters.size()
      );
      refund(_polls.find(courtId)->ownerId);
      print(" (Court_", courtId, " resolved.) ");
    }
  }
}

ACTION opinionbetha::userauth(uint64_t userId) {
  if (_users.find(userId) == _users.end()) {
    _users.emplace(get_self(), [&](auto& p) {
      p.userId = userId;
      p.frozen = 0;
      p.balance = 0;
    });
  }
}

ACTION opinionbetha::refund(uint64_t userId) {
  userauth(userId);
  if (_users.find(userId)->frozen == 0)
    return;
  auto user = _users.find(userId);
  safetransfer(userId, user->frozen);
  print(" (user_", userId, " refund) ");
}

ACTION opinionbetha::illegalmint(uint64_t userId, uint64_t value) {
  userauth(userId);
  _users.modify(_users.find(userId), get_self(), [&](auto& p) {
    p.balance += value;
  });
  print("! ILLEGAL MINTING (FOR TEST PURPOSES ONLY !");
}

bool opinionbetha::hasvoted(uint64_t pollId, uint64_t userId) {
  userauth(userId);
  if (contains(_users.find(userId)->voted, pollId)) 
    return true;
  return false;
}

[[eosio::action]]
bool opinionbetha::safetransfer(uint64_t userId, uint64_t value) {
      userauth(userId);
      auto& user = _users.get(userId);
      if (!transfer(0, userId, value)) {
        transfer(0, userId, user.frozen);
        _users.modify(user, get_self(), [&](auto& p) {
          p.frozen = 0;
        });
        return true;
      }
      _users.modify(user, get_self(), [&](auto& p) {
          p.frozen -= value;;
      });
      
  return true; 
}

ACTION opinionbetha::attachoption(uint64_t pollId, uint64_t optionId) {
  if (_options.find(optionId) == _options.end()) {
    _options.emplace(get_self(), [&](auto& p) {
      p.optionId = optionId;
      p.pollId = pollId;
    });
  }
}


