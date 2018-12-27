#include <iostream>
#include <time.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <sstream>

using namespace std;

#define START "__START__"
#define END "__END__"
#define NICK "__NICK__"
#define MODE_NONE 0
#define MODE_IGNORE_END 1
#define MODE_FORCE_END 2

constexpr unsigned int MINIMUM_OCCURENCE = 2;
constexpr unsigned int MARKOV_LENGTH = 2;
constexpr unsigned int MINIMUM_LENGTH = 5;
constexpr unsigned int SOFT_MAXIMUM_LENGTH = 20;
constexpr unsigned int HARD_MAXIMUM_LENGTH = 30;

auto chieq = [](char c1, char c2) { return toupper(c1) == toupper(c2); };

void clean(string& s) {
  static auto isnotspace = [](char c) { return !isspace(c); };

  // Trim left
  s.erase(s.begin(), find_if(s.begin(), s.end(), isnotspace));

  // Trim right
  s.erase(find_if(s.rbegin(), s.rend(), isnotspace).base(), s.end());

  // Clean bad caracters
  remove_if(s.begin(), s.end(), [](char c) { return c == '\n' || c == '\t' || c == '\r'; });

  // Clean double spaces
  s.erase(unique(s.begin(), s.end(), [](char a, char b) { return a == b && a == ' '; }), s.end());
}

inline void concat(vector<string>& dest, const vector<string>& src) {
  dest.insert(dest.end(), src.begin(), src.end());
}

inline ostream& operator<<(ostream& os, const vector<string>& vec) {
  os << "<";

  bool empty = true;

  for (const string& str : vec) {
    if (!empty) {
      os << " ";
    } else {
      empty = false;
    }

    os << str;
  }

  os << ">";

  return os;
}

class Node;

class Next {
  public:
    pair<const vector<string>, Node>* ptr;
    mutable unsigned int score;

    Next(pair<const vector<string>, Node>* ptr) : ptr(ptr), score(1) {}

    bool operator==(const Next& other) const {
      return ptr == other.ptr;
    }

};

namespace std
{
  template<>
  struct hash<Next>
  {
    size_t operator()(const Next& next) const;
  };

  template<>
  struct hash<vector<string>>
  {
    size_t operator()(const vector<string>& vec) const
    {
      stringstream ss;

      for (const string& str : vec) {
        ss << str;
      }

      return hash<string>()(ss.str());
    }
  };
}

class Node {
  public:
    unordered_set<Next> nexts;
    unsigned int total;

    Node() : total(0) {}

    void increase(pair<const vector<string>, Node>* target) {
      for (const Next& next : nexts) {
        if (next.ptr == target) {
          next.score += 1;
          total += 1;
          return;
        }
      }

      nexts.emplace(target);
      total += 1;
    }

    template <int mode>
    pair<const vector<string>, Node>* randomNext(pair<const vector<string>, Node>* end) {
      if (mode == MODE_FORCE_END) {
        for (const Next& next : nexts) {
          if (next.ptr == end) {
            return end;
          }
        }
      }

      unsigned int total = this->total;

      if (mode == MODE_IGNORE_END) {
        for (const Next& next : nexts) {
          if (next.ptr == end) {
            total -= next.score;
          }
        }
      }

      if (total <= 1) {
        return end;
      }

      int r = rand() % total;

      for (const Next& next : nexts) {
        if (mode == MODE_IGNORE_END && next.ptr == end) {
          continue;
        }

        r -= next.score;

        if (r <= 0) {
          return next.ptr;
        }
      }

      return end;
    }
};

namespace std
{
  size_t hash<Next>::operator()(const Next& next) const
  {
    return hash<Node*>()(&next.ptr->second);
  }
}

class Bot {
  public:
    string nickname;
    unordered_set<string> blacklist;
    bool enabled;
    unordered_map<vector<string>, Node> data;
    pair<const vector<string>, Node>* start;
    pair<const vector<string>, Node>* end;

    Bot(string nickname) : nickname(nickname), blacklist({ nickname }), enabled(false) {
      vector<string> vec = { START };
      start = find(vec);

      vec = { END };
      end = find(vec);
    }

    void clean() {
      bool found = false;

      do {
        for (auto it = data.begin(); it != data.end();) {
          if (it->second.total < MINIMUM_OCCURENCE) {
            it = data.erase(it);
          } else {
            ++it;
          }
        }

        for (auto& pair : data) {
          for (auto it = pair.second.nexts.begin(); it != pair.second.nexts.end();) {
            if (data.find((*it).ptr->first) == data.end()) {
              pair.second.total -= (*it).score;

              if (pair.second.total < MINIMUM_OCCURENCE) {
                found = true;
              }

              it = pair.second.nexts.erase(it);
            } else {
              ++it;
            }
          }
        }
      } while (found);

      data.rehash(0);
    }

    inline pair<const vector<string>, Node>* find(vector<string>& key) {
      data[key];
      return &(*data.find(key));
    }

    vector<string> split(string& str) {
      vector<string> result;

      istringstream iss(str);
      for (string s; iss >> s;) {
        if (s == START || s == END) {
          continue;
        }

        if (equal(s.begin(), s.end(), nickname.begin(), nickname.end(), chieq)) {
          s = NICK;
        }

        result.push_back(s);
      }

      return result;
    }

    inline void learn(Node& from, vector<string>& to) {
      from.increase(find(to));
    }

    inline void learn(vector<string>& from, string& to) {
      vector<string> vec({ to });
      find(from)->second.increase(find(vec));
    }

    inline void learn(vector<string>& from, pair<const vector<string>, Node>* to) {
      find(from)->second.increase(to);
    }

    void learn(string& body) {
      vector<string> words = split(body);

      if (words.size() < MARKOV_LENGTH) {
        return;
      }

      vector<string> history;
      auto itr = words.begin();

      for (unsigned int i = 0; i < MARKOV_LENGTH - 1; ++i) {
        history.push_back(*itr++);
      }

      learn(start->second, history);

      history.insert(history.begin(), START);

      for (unsigned int i = MARKOV_LENGTH - 1; i < words.size(); ++i) {
        learn(history, *itr);
        history.erase(history.begin());
        history.push_back(*itr);
        ++itr;
      }

      learn(history, end);
    }

    void talk() {
      vector<string> result = { START };

      pair<const vector<string>, Node>* next = start->second.randomNext<MODE_IGNORE_END>(end);

      concat(result, next->first);

      while (result.size() < HARD_MAXIMUM_LENGTH) {
        vector<string> last(result.end() - MARKOV_LENGTH, result.end());

        auto itr = data.find(last);

        if (itr == data.end()) {
          break;
        }

        if (result.size() < SOFT_MAXIMUM_LENGTH) {
          next = (*itr).second.randomNext<MODE_NONE>(end);
        } else {
          next = (*itr).second.randomNext<MODE_FORCE_END>(end);
        }

        if (next == end) {
          break;
        }

        concat(result, next->first);
      }

      if (result.size() > 1) {
        cout << result[1];

        for (unsigned int i = 2; i < result.size(); ++i) {
          cout << " " << (result[i] == NICK ? nickname : result[i]);
        }

        cout << endl;
      }
    }
};

int main(int argc ,char **argv) {
  if (argc < 2) {
    cerr << "Not enought arguments" << endl;

    return 1;
  }

  srand(time(NULL));

  Bot bot(argv[1]);

  for (int i = 2; i < argc; ++i) {
    bot.blacklist.insert(argv[i]);
  }

  while (true) {
    string username;
    string body;

    cin >> username;
    getline(cin, body);

    clean(body);

    if (username == "###") {
      if (body == "ENABLE") {
        bot.enabled = true;
      } else if (body == "DISABLE") {
        bot.enabled = false;
      } else if (body == "CLEAN") {
        bot.clean();
      } else if (body == "STOP") {
        break;
      }
    } else if (bot.blacklist.find(username) == bot.blacklist.end()) {
      bot.learn(body);

      if (bot.enabled && search(body.begin(), body.end(), bot.nickname.begin(), bot.nickname.end(), chieq) != body.end()) {
        bot.talk();
      }
    }
  }

  return 0;
}