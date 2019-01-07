#include <iostream>
#include <time.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <sstream>
#include <cstring>

using namespace std;

//************************************
// Constants

#define START "__START__"
#define END "__END__"
#define NICK "__NICK__"
#define MODE_NONE 0
#define MODE_IGNORE_END 1
#define MODE_FORCE_END 2

constexpr unsigned int MARKOV_LENGTH = 3;
constexpr unsigned int SOFT_MINIMUM_LENGTH = 5;
constexpr unsigned int HARD_MINIMUM_LENGTH = 2;
constexpr unsigned int SOFT_MAXIMUM_LENGTH = 15;
constexpr unsigned int HARD_MAXIMUM_LENGTH = 20;
constexpr unsigned int SOFT_TRY = 10;

// Memory usage
constexpr unsigned int MINIMUM_OCCURENCES = 3;
constexpr unsigned int MINIMUM_SCORE = 3;
constexpr unsigned int REHASH_THRESOLD = 100000;
constexpr bool CASE_INSENSITIVE = true;

typedef array<string, MARKOV_LENGTH> strings;

auto chieq = [](char c1, char c2) { return toupper(c1) == toupper(c2); };

//************************************
// Declarations

namespace std {
  template<>
  struct hash<strings> {
    size_t operator()(const strings& v) const;
  };
}

class Node {
  public:
    unsigned int occurences;
    unsigned int total;
    unsigned int end;
    unordered_map<const strings*, unsigned int> nexts;

    Node();

    void increase(const strings& next);

    template <int mode>
    const strings* randomNext();
};

class Bot {
  public:
    string nickname;
    bool enabled;
    unsigned int startsTotal;

    unordered_set<string> blacklist;
    unordered_map<strings, Node> words;
    unordered_set<pair<const strings, Node>*> starts;

    Bot(string nickname);

    void learn(string& message);
    void talk(vector<string>& result);
    void rehash();
    vector<string> split(string& str);
    const strings* randomStart();

    template <bool start>
    void add(strings& from, string& to);

};

//************************************
// Globals

//************************************
// Definitions

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

template <typename T>
inline void concat(vector<string>& dest, const T& src) {
  dest.insert(dest.end(), src.begin(), src.end());
}

template <typename T>
void print(ostream& os, const T& vec) {
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
}

inline ostream& operator<<(ostream& os, const vector<string>& vec) {
  print(os, vec);
  return os;
}

inline ostream& operator<<(ostream& os, const strings& vec) {
  print(os, vec);
  return os;
}

Node::Node() : occurences(0), total(0), end(0) {}

void Node::increase(const strings& next) {
  nexts[&next] += 1;
  total += 1;

  if (next[MARKOV_LENGTH - 1] == END) {
    end += 1;
  }
}

template <int mode>
const strings* Node::randomNext() {
  unsigned int total;

  if (mode == MODE_FORCE_END && end > 0) {
    total = end;
  } else if (mode == MODE_IGNORE_END && end > 0) {
    total = this->total - end;
  } else {
    total = this->total;
  }

  int r = 1 + rand() % total;

  for (auto& p : nexts) {
    if (mode == MODE_FORCE_END && end > 0 && p.first->at(MARKOV_LENGTH - 1) != END) {
      continue;
    }

    if (mode == MODE_IGNORE_END && end > 0 && p.first->at(MARKOV_LENGTH - 1) == END) {
      continue;
    }

    r -= p.second;

    if (r <= 0) {
      return p.first;
    }
  }

  cerr << "randomNext - Returning null: mode " << mode << " r " << r << " total " << total << " end " << end << " this->total " << this->total << endl;

  return NULL;
}

Bot::Bot(string nickname) : nickname(nickname), enabled(false), startsTotal(0), blacklist({ nickname }) {

}

vector<string> Bot::split(string& str) {
  vector<string> result;

  istringstream iss(str);
  for (string s; iss >> s;) {
    if (s == START || s == END || s == NICK) {
      continue;
    }

    if (equal(s.begin(), s.end(), nickname.begin(), nickname.end(), chieq)) {
      s = NICK;
    }

    result.push_back(s);
  }

  return result;
}

void Bot::rehash() {
  bool found;

  cout << "### Before rehash: words.size() " << words.size() << " starts.size() " << starts.size() << " startsTotal " << startsTotal << endl;

  do {
    found = false;

    unordered_set<const strings*> deleted;

    for (auto& p : words) {
      if ((p.first[MARKOV_LENGTH - 1] != END && p.second.total <= 0) || p.second.occurences < MINIMUM_OCCURENCES) {
        deleted.insert(&p.first);

        if (p.first[0] == START) {
          starts.erase(&p);
        }
      }
    }

    for (auto& p : words) {
      if (deleted.find(&p.first) != deleted.end()) {
        continue;
      }

      for (auto it = p.second.nexts.begin(); it != p.second.nexts.end();) {
        if (deleted.find(it->first) != deleted.end() || it->second < MINIMUM_SCORE) {
          p.second.total -= it->second;

          if (p.second.total <= 0) {
            found = true;
          }

          if (it->first->at(MARKOV_LENGTH - 1) == END) {
            p.second.end -= it->second;
          }

          p.second.occurences -= 1;

          if (p.second.occurences < MINIMUM_OCCURENCES) {
            found = true;
          }

          it = p.second.nexts.erase(it);
        } else {
          ++it;
        }
      }
    }

    for (auto it = words.begin(); it != words.end();) {
      if (deleted.find(&it->first) != deleted.end()) {
        it = words.erase(it);
      } else {
        ++it;
      }
    }
  } while (found);

  words.rehash(0);

  startsTotal = 0;

  for (auto& v : starts) {
    startsTotal += v->second.total;
  }

  cout << "### After rehash: words.size() " << words.size() << " starts.size() " << starts.size() << " startsTotal " << startsTotal << endl;
}

template <bool start>
void Bot::add(strings& from, string& to) {
  strings dest;

  for (unsigned int i = 0; i < MARKOV_LENGTH - 1; ++i) {
    dest[i] = from[i + 1];
  }

  dest[MARKOV_LENGTH - 1] = to;

  words[dest].occurences += 1;

  words[from].increase(words.find(dest)->first);

  if (start) {
    words[from].occurences += 1;
    startsTotal += 1;
    starts.insert(&*words.find(from));
  }
}

void Bot::learn(string& message) {
  static string end(END);

  vector<string> words = split(message);

  if (words.size() <= 0 || words.size() < MARKOV_LENGTH - 1) {
    return;
  }

  strings history;
  history[0] = START;

  for (unsigned int i = 0; i < MARKOV_LENGTH - 1; ++i) {
    history[i + 1] = words[i];
  }

  for (unsigned int i = MARKOV_LENGTH - 1; i < words.size(); ++i) {
    if (i == MARKOV_LENGTH - 1) {
      add<true>(history, words[i]);
    } else {
      add<false>(history, words[i]);
    }

    for (unsigned int j = 0; j < MARKOV_LENGTH - 1; ++j) {
      history[j] = history[j + 1];
    }

    history[MARKOV_LENGTH - 1] = words[i];
  }

  add<false>(history, end);

  if (enabled && words.size() > REHASH_THRESOLD) {
    rehash();
  }
}

const strings* Bot::randomStart() {
  int r = 1 + rand() % startsTotal;

  for (const pair<const strings, Node>* p : starts) {
    r -= p->second.total;

    if (r <= 0) {
      return &p->first;
    }
  }

  cerr << "randomStart - Returning null: startsTotal " << startsTotal << " r " << r << endl;

  return NULL;
}

void Bot::talk(vector<string>& result) {
  result.clear();

  const strings* history = randomStart();
  concat(result, *history);

  while (result.size() < HARD_MAXIMUM_LENGTH + 1) {
    Node& node = words[*history];

    if (result.size() < HARD_MINIMUM_LENGTH + 1) {
      history = node.randomNext<MODE_IGNORE_END>();
    } else if (result.size() > SOFT_MAXIMUM_LENGTH + 1) {
      history = node.randomNext<MODE_FORCE_END>();
    } else {
      history = node.randomNext<MODE_NONE>();
    }

    const string& last = history->at(MARKOV_LENGTH - 1);

    if (last == END) {
      break;
    }

    result.push_back(last);

    if (result.size() == HARD_MAXIMUM_LENGTH + 1) {
      result.push_back("...");
    }
  }
}

namespace std {
  size_t hash<strings>::operator()(const strings& v) const {
    stringstream ss;

    for (const string& str : v) {
      ss << str;
    }

    return hash<string>()(ss.str());
  }
}

//************************************
// Main

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

    if (username.length() == 10 && username[0] == '(' && username[9] == ')') {
      cin >> username;

      string useless;
      cin >> useless;
    }

    getline(cin, body);

    clean(body);

    if (username.length() == 0 && body.length() == 0) {
      cin.ignore();
      continue;
    }

    if (username == "###") {
      if (body == "ENABLE") {
        bot.rehash();
        bot.enabled = true;
      } else if (body == "DISABLE") {
        bot.enabled = false;
      } else if (body == "STOP") {
        break;
      }
    } else if (bot.blacklist.find(username) == bot.blacklist.end()) {
      if (CASE_INSENSITIVE) {
        transform(body.begin(), body.end(), body.begin(), ::tolower);
      }

      bot.learn(body);

      if (bot.enabled && search(body.begin(), body.end(), bot.nickname.begin(), bot.nickname.end(), chieq) != body.end()) {
        vector<string> output;

        unsigned int counter = 0;

        do {
          bot.talk(output);
          counter += 1;
        } while (output.size() - 1 < (counter < SOFT_TRY ? SOFT_MINIMUM_LENGTH : HARD_MINIMUM_LENGTH));

        cout << (output[1] == NICK ? username : output[1]);

        for (unsigned int i = 2; i < output.size(); ++i) {
          cout << " " << (output[i] == NICK ? username : output[i]);
        }

        cout << endl;
      }
    }
  }

  return 0;
}