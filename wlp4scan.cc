#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <climits>
const std::string ALPHABET    = ".ALPHABET";
const std::string STATES      = ".STATES";
const std::string TRANSITIONS = ".TRANSITIONS";
const std::string INPUT       = ".INPUT";
const std::string EMPTY       = ".EMPTY";

// Embedded WLP4 DFA
std::string wlp4_dfa_string = R"(.ALPHABET
a-z A-Z 0-9 ( ) { } = ! < > + - * / % , ; [ ] &   
.STATES
start
alpha! id!
zero! nonZeroDigit! dec!
lparen! rparen! lbrace! rbrace!
equals! becomes! eq!
exclamation ne!
less! lt! le!
greater! gt! ge!
plus! minus! star! slashStart! pct! comma! semi!
lbrack! rbrack! amp!
.TRANSITIONS
start a-z A-Z alpha
start 0 zero
start 1-9 nonZeroDigit
start ( lparen
start ) rparen
start { lbrace
start } rbrace
start = equals
start ! exclamation
start < less
start > greater
start + plus
start - minus
start * star
start / slashStart
start % pct
start , comma
start ; semi
start [ lbrack
start ] rbrack
start & amp
alpha a-z A-Z 0-9 id
id a-z A-Z 0-9 id
nonZeroDigit 0-9 dec
dec 0-9 dec
equals = eq
exclamation = ne
less = le
greater = ge
.INPUT
)";

bool isChar(std::string s) {
  return s.length() == 1;
}

bool isRange(std::string s) {
  return s.length() == 3 && s[1] == '-';
}

bool isKeyword(const std::string& lexeme) {
  static std::unordered_set<std::string> keywords = {
    "wain", "int", "if", "else", "while", "println", "putchar", 
    "getchar", "return", "NULL", "new", "delete"
  };
  return keywords.count(lexeme) > 0;
}

std::string getKeywordType(const std::string& lexeme) {
  if (lexeme == "wain") return "WAIN";
  if (lexeme == "int") return "INT";
  if (lexeme == "if") return "IF";
  if (lexeme == "else") return "ELSE";
  if (lexeme == "while") return "WHILE";
  if (lexeme == "println") return "PRINTLN";
  if (lexeme == "putchar") return "PUTCHAR";
  if (lexeme == "getchar") return "GETCHAR";
  if (lexeme == "return") return "RETURN";
  if (lexeme == "NULL") return "NULL";
  if (lexeme == "new") return "NEW";
  if (lexeme == "delete") return "DELETE";
  return "ID";
}

std::string getTokenType(const std::string& state, const std::string& lexeme) {
  if (state == "id" || state == "alpha") {
    if (isKeyword(lexeme)) {
      return getKeywordType(lexeme);
    }
    return "ID";
  }
  if (state == "dec" || state == "zero" || state == "nonZeroDigit") return "NUM";
  if (state == "lparen") return "LPAREN";
  if (state == "rparen") return "RPAREN";
  if (state == "lbrace") return "LBRACE";
  if (state == "rbrace") return "RBRACE";
  if (state == "equals") return "BECOMES";
  if (state == "becomes") return "BECOMES";
  if (state == "eq") return "EQ";
  if (state == "ne") return "NE";
  if (state == "less") return "LT";
  if (state == "lt") return "LT";
  if (state == "greater") return "GT";
  if (state == "gt") return "GT";
  if (state == "le") return "LE";
  if (state == "ge") return "GE";
  if (state == "plus") return "PLUS";
  if (state == "minus") return "MINUS";
  if (state == "star") return "STAR";
  if (state == "slashStart") return "SLASH";
  if (state == "pct") return "PCT";
  if (state == "comma") return "COMMA";
  if (state == "semi") return "SEMI";
  if (state == "lbrack") return "LBRACK";
  if (state == "rbrack") return "RBRACK";
  if (state == "amp") return "AMP";
  return "UNKNOWN";
}

bool isValidNumber(const std::string& numStr) {
  try {
    long long num = std::stoll(numStr);
    return num <= 2147483647LL;
  } catch (...) {
    return false;
  }
}

int main() {
  std::stringstream dfaStream(wlp4_dfa_string);
  std::istream& in = dfaStream;

  std::string s;

  std::unordered_set<char> alphabet;
  std::unordered_map<std::string, std::unordered_map<char, std::string>> transitions;
  std::unordered_set<std::string> acceptingStates;
  std::string initialState;

  std::getline(in, s); // Alphabet section (skip header)
  // Read characters or ranges separated by whitespace
  while(in >> s) {
    if (s == STATES) {
      break;
    } else {
      if (isChar(s)) {
        //// Variable 's[0]' is an alphabet symbol
        alphabet.insert(s[0]);
      } else if (isRange(s)) {
        for(char c = s[0]; c <= s[2]; ++c) {
          //// Variable 'c' is an alphabet symbol
          alphabet.insert(c);
        }
      }
    }
  }

  std::getline(in, s); // States section (skip header)
  // Read states separated by whitespace
  while(in >> s) {
    if (s == TRANSITIONS) {
      break;
    } else {
      static bool initial = true;
      bool accepting = false;
      if (s.back() == '!' && !isChar(s)) {
        accepting = true;
        s.pop_back();
      }
      //// Variable 's' contains the name of a state
      if (initial) {
        //// The state is initial
        initial = false;
        initialState = s;
      }
      if (accepting) {
        //// The state is accepting
        acceptingStates.insert(s);
      }
      transitions.insert({s, std::unordered_map<char, std::string>()});
    }
  }

  std::getline(in, s); // Transitions section (skip header)
  // Read transitions line-by-line
  while(std::getline(in, s)) {
    if (s == INPUT) {
      // Note: Since we're reading line by line, once we encounter the
      // input header, we will already be on the line after the header
      break;
    } else {
      std::string fromState, symbols, toState;
      std::istringstream line(s);
      std::vector<std::string> lineVec;
      while(line >> s) {
        lineVec.push_back(s);
      }
      fromState = lineVec.front();
      toState = lineVec.back();
      for(int i = 1; i < lineVec.size()-1; ++i) {
        std::string s = lineVec[i];
        if (isChar(s)) {
          symbols += s;
        } else if (isRange(s)) {
          for(char c = s[0]; c <= s[2]; ++c) {
            symbols += c;
          }
        }
      }
      for ( char c : symbols ) {
        //// There is a transition from 'fromState' to 'toState' on 'c'
        transitions[fromState][c] = toState;
      }
    }
  }

  // Read WLP4 source from stdin and tokenize
  std::string entireInput;
  std::string line;
  std::istream& wlp4Input = std::cin;
  
  while(std::getline(wlp4Input, line)) {
    entireInput += line;
    if (wlp4Input.peek() != EOF) {
      entireInput += '\n';
    }
  }

  std::istringstream inputStream(entireInput);
  std::string currentLine;
  
  while(std::getline(inputStream, currentLine)) {
    std::string processLine = currentLine;
    
    size_t pos = 0;
    while (pos < processLine.length()) {
      // Skip whitespace
      while (pos < processLine.length() && (processLine[pos] == ' ' || processLine[pos] == '\t')) {
        pos++;
      }
      
      if (pos >= processLine.length()) break;
      
      // Check for comments
      if (pos < processLine.length() - 1 && processLine[pos] == '/' && processLine[pos + 1] == '/') {
        // Skip rest of line (comment)
        break;
      }
      
      std::string x = processLine.substr(pos);
      std::string p = initialState;
      std::string t = ""; // token read so far
      
      while (!x.empty()) {
        char a = x[0]; // peek
        
        if (transitions.find(p) != transitions.end() && 
            transitions[p].find(a) != transitions[p].end()) {
          t += a;
          x = x.substr(1);
          p = transitions[p][a];
        } else {
          // stuck: no arrow out of current state on next character
          if (acceptingStates.count(p) > 0) {
            std::string tokenType = getTokenType(p, t);
            
            // Check numeric range for NUM tokens
            if (tokenType == "NUM" && !isValidNumber(t)) {
              std::cerr << "ERROR" << std::endl;
              return 1;
            }
            
            std::cout << tokenType << " " << t << std::endl;
            pos += t.length();
            break;
          } else {
            std::cerr << "ERROR" << std::endl;
            return 1;
          }
        }
      }
      
      // Handle end of line
      if (x.empty() && acceptingStates.count(p) > 0) {
        std::string tokenType = getTokenType(p, t);
        
        // Check numeric range for NUM tokens
        if (tokenType == "NUM" && !isValidNumber(t)) {
          std::cerr << "ERROR" << std::endl;
          return 1;
        }
        
        std::cout << tokenType << " " << t << std::endl;
        pos += t.length();
      } else if (x.empty() && acceptingStates.count(p) == 0) {
        std::cerr << "ERROR" << std::endl;
        return 1;
      }
    }
  }

  return 0;
} 
