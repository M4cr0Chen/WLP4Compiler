#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <functional>

using namespace std;

// Global procedure tables: procedure_name -> (param_types, symbol_table)
map<string, pair<vector<string>, map<string, string>>> tables;

// Current procedure name being analyzed
string currentProcedure;

// Tree node structure to represent the parse tree
struct TreeNode {
    string rule;           // For nonterminals: the production rule
    string tokenKind;      // For terminals: the token kind
    string lexeme;         // For terminals: the lexeme
    string type;           // Type annotation for expressions
    vector<shared_ptr<TreeNode>> children;
    
    TreeNode(const string& r = "") : rule(r) {}
    
    bool isTerminal() const {
        return !tokenKind.empty();
    }
    
    bool isExpression() const {
        // Expression nodes are terminals (NUM, NULL, ID) or nonterminals (expr, term, factor, lvalue)
        if (isTerminal()) {
            return tokenKind == "NUM" || tokenKind == "NULL" || tokenKind == "ID";
        } else {
            string lhs = rule.substr(0, rule.find(' '));
            return lhs == "expr" || lhs == "term" || lhs == "factor" || lhs == "lvalue";
        }
    }
};

// Function to read a line and split it into tokens
vector<string> split(const string& line) {
    vector<string> tokens;
    istringstream iss(line);
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to parse the .wlp4i format and build the parse tree
shared_ptr<TreeNode> parseTree() {
    string line;
    if (!getline(cin, line)) {
        return nullptr;
    }
    
    vector<string> tokens = split(line);
    if (tokens.empty()) {
        return nullptr;
    }
    
    auto node = make_shared<TreeNode>();
    
    // Check if this is a terminal node
    // Terminals are: BOF BOF, EOF EOF, or TOKEN lexeme, or .EMPTY
    if ((tokens.size() == 2 && tokens[0] == "BOF" && tokens[1] == "BOF") ||
        (tokens.size() == 2 && tokens[0] == "EOF" && tokens[1] == "EOF") ||
        (tokens.size() == 2 && tokens[0] == "NULL" && tokens[1] == "NULL") ||
        (tokens.size() == 2 && tokens[0] != tokens[1] && 
         (tokens[0] == "INT" || tokens[0] == "WAIN" || tokens[0] == "LPAREN" || 
          tokens[0] == "COMMA" || tokens[0] == "RPAREN" || tokens[0] == "LBRACE" ||
          tokens[0] == "RBRACE" || tokens[0] == "RETURN" || tokens[0] == "SEMI" || 
          tokens[0] == "STAR" || tokens[0] == "NUM" || tokens[0] == "NULL" || 
          tokens[0] == "ID" || tokens[0] == "NEW" || tokens[0] == "LBRACK" ||
          tokens[0] == "RBRACK" || tokens[0] == "AMP" || tokens[0] == "PLUS" ||
          tokens[0] == "MINUS" || tokens[0] == "SLASH" || tokens[0] == "PCT" ||
          tokens[0] == "IF" || tokens[0] == "ELSE" || tokens[0] == "WHILE" ||
          tokens[0] == "PRINTLN" || tokens[0] == "PUTCHAR" || tokens[0] == "DELETE" ||
          tokens[0] == "BECOMES" || tokens[0] == "EQ" || tokens[0] == "NE" ||
          tokens[0] == "LT" || tokens[0] == "LE" || tokens[0] == "GT" ||
          tokens[0] == "GE" || tokens[0] == "GETCHAR")) ||
        (tokens.size() == 1 && tokens[0] == ".EMPTY")) {
        // Terminal node
        node->tokenKind = tokens[0];
        if (tokens.size() > 1) {
            node->lexeme = tokens[1];
        }
    } else {
        // Nonterminal node - it's a production rule
        node->rule = line;
        
        // Extract the right-hand side symbols
        // Format: "lhs symbol1 symbol2 symbol3 ..."
        vector<string> rhsSymbols;
        if (tokens.size() > 1) {
            for (size_t i = 1; i < tokens.size(); i++) {
                rhsSymbols.push_back(tokens[i]);
            }
        }
        
        // Parse children for each symbol in RHS
        for (const string& symbol : rhsSymbols) {
            if (symbol != ".EMPTY") {
                auto child = parseTree();
                if (child) {
                    node->children.push_back(child);
                }
            }
        }
    }
    
    return node;
}

// Function to find a node with a specific rule pattern
shared_ptr<TreeNode> findNode(shared_ptr<TreeNode> root, const string& pattern) {
    if (!root) return nullptr;
    
    if (!root->isTerminal() && root->rule.find(pattern) != string::npos) {
        return root;
    }
    
    for (auto child : root->children) {
        auto result = findNode(child, pattern);
        if (result) return result;
    }
    
    return nullptr;
}

// Function to get the type from a type node
string getType(shared_ptr<TreeNode> typeNode) {
    if (!typeNode) return "";
    
    // Look for INT and STAR tokens in the type subtree
    bool hasInt = false;
    bool hasStar = false;
    
    function<void(shared_ptr<TreeNode>)> traverse = [&](shared_ptr<TreeNode> node) {
        if (!node) return;
        if (node->tokenKind == "INT") hasInt = true;
        if (node->tokenKind == "STAR") hasStar = true;
        for (auto child : node->children) {
            traverse(child);
        }
    };
    
    traverse(typeNode);
    
    if (hasInt && hasStar) return "int*";
    if (hasInt) return "int";
    return "";
}

// Forward declarations
string analyzeExpression(shared_ptr<TreeNode> exprNode);
bool analyzeStatements(shared_ptr<TreeNode> stmtsNode);
bool analyzeStatement(shared_ptr<TreeNode> stmtNode);

// Function to analyze expression type and annotate
string analyzeExpression(shared_ptr<TreeNode> exprNode) {
    if (!exprNode) return "";
    
    if (exprNode->isTerminal()) {
        // Terminal expressions
        if (exprNode->tokenKind == "NUM") {
            exprNode->type = "int";
            return "int";
        } else if (exprNode->tokenKind == "NULL") {
            exprNode->type = "int*";
            return "int*";
        } else if (exprNode->tokenKind == "ID") {
            // Variable or procedure reference
            string name = exprNode->lexeme;
            
            // Don't set types on procedure name declarations or procedure calls
            // This will be handled later by setIdTypes()
            
            auto& currentSymbolTable = tables[currentProcedure].second;
            if (currentSymbolTable.find(name) != currentSymbolTable.end()) {
                exprNode->type = currentSymbolTable[name];
                return currentSymbolTable[name];
            } else if (tables.find(name) != tables.end()) {
                // Procedure name used as expression - error
                return "";
            } else {
                // Undeclared variable
                return "";
            }
        }
    } else {
        // Non-terminal expressions
        string rule = exprNode->rule;
        
        if (rule.find("expr PLUS term") != string::npos) {
            // Addition: int + int -> int, int* + int -> int*, int + int* -> int*
            if (exprNode->children.size() < 3) return "";
            string leftType = analyzeExpression(exprNode->children[0]);
            string rightType = analyzeExpression(exprNode->children[2]);
            if (leftType.empty() || rightType.empty()) return "";
            
            if ((leftType == "int" && rightType == "int") ||
                (leftType == "int*" && rightType == "int") ||
                (leftType == "int" && rightType == "int*")) {
                string resultType = (leftType == "int*" || rightType == "int*") ? "int*" : "int";
                exprNode->type = resultType;
                return resultType;
            }
            return "";
            
        } else if (rule.find("expr MINUS term") != string::npos) {
            // Subtraction: int - int -> int, int* - int -> int*, int* - int* -> int
            if (exprNode->children.size() < 3) return "";
            string leftType = analyzeExpression(exprNode->children[0]);
            string rightType = analyzeExpression(exprNode->children[2]);
            if (leftType.empty() || rightType.empty()) return "";
            
            if ((leftType == "int" && rightType == "int") ||
                (leftType == "int*" && rightType == "int") ||
                (leftType == "int*" && rightType == "int*")) {
                string resultType;
                if (leftType == "int*" && rightType == "int*") resultType = "int";
                else if (leftType == "int*") resultType = "int*";
                else resultType = "int";
                exprNode->type = resultType;
                return resultType;
            }
            return "";
            
        } else if (rule.find("term STAR factor") != string::npos ||
                   rule.find("term SLASH factor") != string::npos ||
                   rule.find("term PCT factor") != string::npos) {
            // Multiplication, division, modulo: int op int -> int
            if (exprNode->children.size() < 3) return "";
            string leftType = analyzeExpression(exprNode->children[0]);
            string rightType = analyzeExpression(exprNode->children[2]);
            if (leftType.empty() || rightType.empty()) return "";
            
            if (leftType == "int" && rightType == "int") {
                exprNode->type = "int";
                return "int";
            }
            return "";
            
        } else if (rule.find("factor AMP lvalue") != string::npos) {
            // Address-of: &lvalue -> int*
            if (exprNode->children.size() < 2) return "";
            string lvalueType = analyzeExpression(exprNode->children[1]);
            if (lvalueType.empty()) return "";
            
            if (lvalueType == "int") {
                exprNode->type = "int*";
                return "int*";
            }
            return "";
            
        } else if (rule.find("factor STAR factor") != string::npos) {
            // Dereference: *factor -> int (if factor is int*)
            if (exprNode->children.size() < 2) return "";
            string factorType = analyzeExpression(exprNode->children[1]);
            if (factorType.empty()) return "";
            
            if (factorType == "int*") {
                exprNode->type = "int";
                return "int";
            }
            return "";
            
        } else if (rule.find("factor NEW INT LBRACK expr RBRACK") != string::npos) {
            // New array: new int[expr] -> int* (if expr is int)
            if (exprNode->children.size() < 4) return "";
            string exprType = analyzeExpression(exprNode->children[3]);
            if (exprType.empty()) return "";
            
            if (exprType == "int") {
                exprNode->type = "int*";
                return "int*";
            }
            return "";
            
        } else if (rule.find("factor ID LPAREN RPAREN") != string::npos) {
            // Procedure call with no arguments
            string procName = exprNode->children[0]->lexeme;
            if (tables.find(procName) == tables.end()) {
                return ""; // Undeclared procedure (call before declaration)
            }
            
            auto& paramTypes = tables[procName].first;
            if (!paramTypes.empty()) {
                return ""; // Wrong number of arguments
            }
            
            exprNode->type = "int";
            return "int";
            
        } else if (rule.find("factor ID LPAREN arglist RPAREN") != string::npos) {
            // Procedure call with arguments
            string procName = exprNode->children[0]->lexeme;
            if (tables.find(procName) == tables.end()) {
                return ""; // Undeclared procedure (call before declaration)
            }
            
            // Analyze arguments
            vector<string> argTypes;
            function<void(shared_ptr<TreeNode>)> collectArgs = [&](shared_ptr<TreeNode> arglistNode) {
                if (!arglistNode || arglistNode->rule.find("arglist") == string::npos) return;
                
                if (arglistNode->rule.find("expr COMMA arglist") != string::npos) {
                    string argType = analyzeExpression(arglistNode->children[0]);
                    if (!argType.empty()) argTypes.push_back(argType);
                    collectArgs(arglistNode->children[2]);
                } else if (arglistNode->rule.find("arglist expr") != string::npos) {
                    string argType = analyzeExpression(arglistNode->children[0]);
                    if (!argType.empty()) argTypes.push_back(argType);
                }
            };
            collectArgs(exprNode->children[2]);
            
            auto& paramTypes = tables[procName].first;
            if (argTypes.size() != paramTypes.size()) {
                return ""; // Wrong number of arguments
            }
            
            for (size_t i = 0; i < argTypes.size(); i++) {
                if (argTypes[i] != paramTypes[i]) {
                    return ""; // Argument type mismatch
                }
            }
            
            exprNode->type = "int";
            return "int";
            
        } else if (rule.find("factor GETCHAR LPAREN RPAREN") != string::npos) {
            // getchar() -> int
            exprNode->type = "int";
            return "int";
            
        } else if (rule.find("test expr") != string::npos) {
            // Test expressions (comparisons): EQ, NE, LT, LE, GE, GT
            // Format: test expr OP expr
            if (exprNode->children.size() < 3) return "";
            string leftType = analyzeExpression(exprNode->children[0]);
            string rightType = analyzeExpression(exprNode->children[2]);
            if (leftType.empty() || rightType.empty()) return "";
            
            // Both operands must have the same type
            if (leftType == rightType) {
                exprNode->type = "int";  // Comparisons return boolean (int in WLP4)
                return "int";
            }
            return "";
            
        } else if (rule.find("lvalue STAR factor") != string::npos) {
            // Dereference lvalue: *factor
            string factorType = analyzeExpression(exprNode->children[1]);
            if (factorType.empty()) return "";
            
            if (factorType == "int*") {
                exprNode->type = "int";
                return "int";
            }
            return "";
            
        } else if (rule.find("lvalue LPAREN lvalue RPAREN") != string::npos) {
            // Parenthesized lvalue
            string lvalueType = analyzeExpression(exprNode->children[1]);
            if (!lvalueType.empty()) {
                exprNode->type = lvalueType;
                return lvalueType;
            }
            return "";
            
        } else if (rule.find("factor LPAREN expr RPAREN") != string::npos) {
            // Parenthesized expression
            string exprType = analyzeExpression(exprNode->children[1]);
            if (!exprType.empty()) {
                exprNode->type = exprType;
                return exprType;
            }
            return "";
            
        } else {
            // Default: propagate type from first child
            for (auto child : exprNode->children) {
                string childType = analyzeExpression(child);
                if (!childType.empty()) {
                    exprNode->type = childType;
                    return childType;
                }
            }
        }
    }
    
    return "";
}

// Function to analyze statements
bool analyzeStatements(shared_ptr<TreeNode> stmtsNode) {
    if (!stmtsNode) return true;
    
    if (stmtsNode->rule.find("statements statement") != string::npos) {
        return analyzeStatements(stmtsNode->children[0]) && 
               analyzeStatement(stmtsNode->children[1]);
    } else if (stmtsNode->rule.find("statements .EMPTY") != string::npos ||
               stmtsNode->tokenKind == ".EMPTY") {
        return true;
    }
    
    return true;
}

// Function to analyze a single statement
bool analyzeStatement(shared_ptr<TreeNode> stmtNode) {
    if (!stmtNode) return true;
    
    string rule = stmtNode->rule;
    
    if (rule.find("statement lvalue BECOMES expr SEMI") != string::npos) {
        // Assignment statement
        string lvalueType = analyzeExpression(stmtNode->children[0]);
        string exprType = analyzeExpression(stmtNode->children[2]);
        
        if (lvalueType.empty() || exprType.empty() || lvalueType != exprType) {
            return false; // Type mismatch
        }
        
    } else if (rule.find("statement IF LPAREN test RPAREN") != string::npos) {
        // If statement
        shared_ptr<TreeNode> testNode = stmtNode->children[2];
        string testType = analyzeExpression(testNode);
        if (testType != "int") return false; // Test must be int (boolean)
        
        // Analyze both branches
        shared_ptr<TreeNode> thenStmts = nullptr, elseStmts = nullptr;
        for (auto child : stmtNode->children) {
            if (child->rule.find("statements") != string::npos) {
                if (!thenStmts) thenStmts = child;
                else elseStmts = child;
            }
        }
        
        return analyzeStatements(thenStmts) && analyzeStatements(elseStmts);
        
    } else if (rule.find("statement WHILE LPAREN test RPAREN") != string::npos) {
        // While loop
        shared_ptr<TreeNode> testNode = stmtNode->children[2];
        string testType = analyzeExpression(testNode);
        if (testType != "int") return false; // Test must be int (boolean)
        
        // Analyze body
        for (auto child : stmtNode->children) {
            if (child->rule.find("statements") != string::npos) {
                return analyzeStatements(child);
            }
        }
        
    } else if (rule.find("statement PRINTLN LPAREN expr RPAREN SEMI") != string::npos) {
        // Print statement
        string exprType = analyzeExpression(stmtNode->children[2]);
        if (exprType != "int") return false; // Must print int
        
    } else if (rule.find("statement PUTCHAR LPAREN expr RPAREN SEMI") != string::npos) {
        // Putchar statement
        string exprType = analyzeExpression(stmtNode->children[2]);
        if (exprType != "int") return false; // Must print int
        
    } else if (rule.find("statement DELETE LBRACK RBRACK expr SEMI") != string::npos) {
        // Delete statement
        string exprType = analyzeExpression(stmtNode->children[3]);
        if (exprType != "int*") return false; // Must delete int*
    }
    
    return true;
}



// Function to analyze a single procedure
bool analyzeProcedure(shared_ptr<TreeNode> procNode) {
    
    // Determine current procedure name
    if (procNode->rule.find("main INT WAIN") != string::npos) {
        currentProcedure = "wain";
        
        // Extract parameters
        vector<shared_ptr<TreeNode>> paramDcls;
        function<void(shared_ptr<TreeNode>, bool&)> findParams = [&](shared_ptr<TreeNode> node, bool& foundBody) {
            if (!node || foundBody) return;
            if (node->tokenKind == "LBRACE") {
                foundBody = true;
                return;
            }
            if (node->rule.find("dcl type ID") != string::npos) {
                paramDcls.push_back(node);
            }
            for (auto child : node->children) {
                findParams(child, foundBody);
            }
        };
        
        bool foundBody = false;
        findParams(procNode, foundBody);
        
        // Add parameters to symbol table
        auto& currentSymbolTable = tables[currentProcedure].second;
        for (auto dcl : paramDcls) {
            string type, name;
            for (auto child : dcl->children) {
                if (child->rule.find("type") != string::npos) {
                    type = getType(child);
                } else if (child->tokenKind == "ID") {
                    name = child->lexeme;
                }
            }
            if (currentSymbolTable.find(name) != currentSymbolTable.end()) {
                return false; // Duplicate parameter
            }
            currentSymbolTable[name] = type;
        }
        
        // Process local declarations
        function<bool(shared_ptr<TreeNode>)> processDecls = [&](shared_ptr<TreeNode> node) -> bool {
            if (!node) return true;
            
            if (node->rule.find("dcls dcl BECOMES") != string::npos) {
                shared_ptr<TreeNode> dclNode = nullptr;
                shared_ptr<TreeNode> valueNode = nullptr;
                
                for (auto child : node->children) {
                    if (child->rule.find("dcl type ID") != string::npos) {
                        dclNode = child;
                    }
                }
                
                // Find the expression node (after BECOMES)
                shared_ptr<TreeNode> exprNode = nullptr;
                for (size_t i = 0; i < node->children.size() - 1; i++) {
                    if (node->children[i]->tokenKind == "BECOMES") {
                        exprNode = node->children[i + 1];
                        break;
                    }
                }
                
                if (!dclNode || !exprNode) return false;
                
                string varType, varName;
                for (auto child : dclNode->children) {
                    if (child->rule.find("type") != string::npos) {
                        varType = getType(child);
                    } else if (child->tokenKind == "ID") {
                        varName = child->lexeme;
                    }
                }
                
                // Analyze the initialization expression to get its type
                string valueType = analyzeExpression(exprNode);
                
                if (valueType.empty() || varType != valueType) return false;
                if (currentSymbolTable.find(varName) != currentSymbolTable.end()) return false;
                
                currentSymbolTable[varName] = varType;
            }
            
            for (auto child : node->children) {
                if (!processDecls(child)) return false;
            }
            return true;
        };
        
        if (!processDecls(procNode)) return false;
        
        // Analyze statements
        for (auto child : procNode->children) {
            if (child->rule.find("statements") != string::npos) {
                if (!analyzeStatements(child)) return false;
            }
        }
        
        // Analyze return expression
        shared_ptr<TreeNode> returnExpr = nullptr;
        function<void(shared_ptr<TreeNode>)> findReturn = [&](shared_ptr<TreeNode> node) {
            if (!node || returnExpr) return;
            
            // Look for the specific pattern: RETURN expr SEMI (anywhere in children)
            for (size_t i = 0; i + 2 < node->children.size(); i++) {
                if (node->children[i]->tokenKind == "RETURN" &&
                    node->children[i + 1]->rule.find("expr") != string::npos &&
                    node->children[i + 2]->tokenKind == "SEMI") {
                    returnExpr = node->children[i + 1];
                    return;
                }
            }
            
            for (auto child : node->children) {
                findReturn(child);
            }
        };
        findReturn(procNode);
        
        if (!returnExpr) return false;
        
        string returnType = analyzeExpression(returnExpr);
        if (returnType != "int") return false;
        
    } else if (procNode->rule.find("procedure INT ID") != string::npos) {
        // Analyze regular procedure (similar to wain but different parameter handling)
        string procName = procNode->children[1]->lexeme;
        currentProcedure = procName;
        
        // Add parameters to symbol table
        auto& currentSymbolTable = tables[currentProcedure].second;
        bool paramError = false;
        for (auto child : procNode->children) {
            if (child->rule.find("params") != string::npos) {
                function<void(shared_ptr<TreeNode>)> addParams = [&](shared_ptr<TreeNode> paramsNode) {
                    if (!paramsNode || paramsNode->tokenKind == ".EMPTY" || paramError) return;
                    
                    if (paramsNode->rule.find("params paramlist") != string::npos) {
                        // Go to paramlist
                        addParams(paramsNode->children[0]);
                    } else if (paramsNode->rule.find("paramlist dcl COMMA paramlist") != string::npos) {
                        // First dcl
                        auto dclNode = paramsNode->children[0];
                        string type, name;
                        for (auto dclChild : dclNode->children) {
                            if (dclChild->rule.find("type") != string::npos) {
                                type = getType(dclChild);
                            } else if (dclChild->tokenKind == "ID") {
                                name = dclChild->lexeme;
                            }
                        }
                        // Check for duplicate parameter
                        if (currentSymbolTable.find(name) != currentSymbolTable.end()) {
                            cerr << "ERROR: In procedure [" << procName << "]: Duplicate variable name: " << name << endl;
                            paramError = true;
                            return;
                        }
                        currentSymbolTable[name] = type;
                        // Then process remaining parameters
                        addParams(paramsNode->children[2]);
                    } else if (paramsNode->rule.find("paramlist dcl") != string::npos) {
                        // Single dcl
                        auto dclNode = paramsNode->children[0];
                        string type, name;
                        for (auto dclChild : dclNode->children) {
                            if (dclChild->rule.find("type") != string::npos) {
                                type = getType(dclChild);
                            } else if (dclChild->tokenKind == "ID") {
                                name = dclChild->lexeme;
                            }
                        }
                        // Check for duplicate parameter
                        if (currentSymbolTable.find(name) != currentSymbolTable.end()) {
                            cerr << "ERROR: In procedure [" << procName << "]: Duplicate variable name: " << name << endl;
                            paramError = true;
                            return;
                        }
                        currentSymbolTable[name] = type;
                    }
                };
                addParams(child);
                break;
            }
        }
        
        if (paramError) {
            return false;
        }
        
        // Process local declarations (similar to wain)
        function<bool(shared_ptr<TreeNode>)> processDecls = [&](shared_ptr<TreeNode> node) -> bool {
            if (!node) return true;
            
            if (node->rule.find("dcls dcl BECOMES") != string::npos) {
                shared_ptr<TreeNode> dclNode = nullptr;
                shared_ptr<TreeNode> exprNode = nullptr;
                
                for (auto child : node->children) {
                    if (child->rule.find("dcl type ID") != string::npos) {
                        dclNode = child;
                    }
                }
                
                // Find the expression node (after BECOMES)
                for (size_t i = 0; i < node->children.size() - 1; i++) {
                    if (node->children[i]->tokenKind == "BECOMES") {
                        exprNode = node->children[i + 1];
                        break;
                    }
                }
                
                if (!dclNode || !exprNode) {
                    return false;
                }
                
                string varType, varName;
                for (auto child : dclNode->children) {
                    if (child->rule.find("type") != string::npos) {
                        varType = getType(child);
                    } else if (child->tokenKind == "ID") {
                        varName = child->lexeme;
                    }
                }
                
                // Analyze the initialization expression to get its type
                string valueType = analyzeExpression(exprNode);
                
                if (valueType.empty() || varType != valueType) {
                    return false;
                }
                if (currentSymbolTable.find(varName) != currentSymbolTable.end()) {
                    return false;
                }
                
                currentSymbolTable[varName] = varType;
            }
            
            for (auto child : node->children) {
                if (!processDecls(child)) return false;
            }
            return true;
        };
        
        if (!processDecls(procNode)) {
            return false;
        }
        
        // Analyze statements
        for (auto child : procNode->children) {
            if (child->rule.find("statements") != string::npos) {
                if (!analyzeStatements(child)) {
                    return false;
                }
            }
        }
        
        // Analyze return expression
        shared_ptr<TreeNode> returnExpr = nullptr;
        function<void(shared_ptr<TreeNode>)> findReturn = [&](shared_ptr<TreeNode> node) {
            if (!node || returnExpr) return;
            
            // Look for the specific pattern: RETURN expr SEMI (anywhere in children)
            for (size_t i = 0; i + 2 < node->children.size(); i++) {
                if (node->children[i]->tokenKind == "RETURN" &&
                    node->children[i + 1]->rule.find("expr") != string::npos &&
                    node->children[i + 2]->tokenKind == "SEMI") {
                    returnExpr = node->children[i + 1];
                    return;
                }
            }
            
            for (auto child : node->children) {
                findReturn(child);
            }
        };
        findReturn(procNode);
        
        if (!returnExpr) {
            return false;
        }
        
        string returnType = analyzeExpression(returnExpr);
        if (returnType != "int") {
            return false;
        }
    }
    
    return true;
}

// Main semantic analysis function
bool semanticAnalysis(shared_ptr<TreeNode> root) {
    
    // Analyze procedures in declaration order, building the procedure table as we go
    function<bool(shared_ptr<TreeNode>)> analyzeInOrder = [&](shared_ptr<TreeNode> node) -> bool {
        if (!node) return true;
        
        if (node->rule.find("procedure INT ID") != string::npos ||
            node->rule.find("main INT WAIN") != string::npos) {
            
            // First, add this procedure's signature to the table
            string procName;
            vector<string> paramTypes;
            
            if (node->rule.find("main INT WAIN") != string::npos) {
                procName = "wain";
                
                // Find parameter types for wain
                function<void(shared_ptr<TreeNode>, bool&)> findParams = [&](shared_ptr<TreeNode> n, bool& foundBody) {
                    if (!n || foundBody) return;
                    if (n->tokenKind == "LBRACE") {
                        foundBody = true;
                        return;
                    }
                    if (n->rule.find("dcl type ID") != string::npos) {
                        for (auto child : n->children) {
                            if (child->rule.find("type") != string::npos) {
                                string paramType = getType(child);
                                paramTypes.push_back(paramType);
                                break;
                            }
                        }
                    }
                    for (auto child : n->children) {
                        findParams(child, foundBody);
                    }
                };
                
                bool foundBody = false;
                findParams(node, foundBody);
                
                if (paramTypes.size() != 2 || paramTypes[1] != "int") {
                    return false;
                }
            } else {
                procName = node->children[1]->lexeme;
                
                // Check for duplicate declaration
                if (tables.find(procName) != tables.end()) {
                    return false;
                }
                
                // Extract parameter types
                for (auto child : node->children) {
                    if (child->rule.find("params") != string::npos) {
                        function<void(shared_ptr<TreeNode>)> collectParams = [&](shared_ptr<TreeNode> paramsNode) {
                            if (!paramsNode || paramsNode->tokenKind == ".EMPTY") {
                                return;
                            }
                            
                            if (paramsNode->rule.find("params paramlist") != string::npos) {
                                // Go to paramlist
                                collectParams(paramsNode->children[0]);
                            } else if (paramsNode->rule.find("paramlist dcl COMMA paramlist") != string::npos) {
                                // First dcl
                                auto dclNode = paramsNode->children[0];
                                for (auto dclChild : dclNode->children) {
                                    if (dclChild->rule.find("type") != string::npos) {
                                        string paramType = getType(dclChild);
                                        paramTypes.push_back(paramType);
                                        break;
                                    }
                                }
                                // Then process remaining parameters
                                collectParams(paramsNode->children[2]);
                            } else if (paramsNode->rule.find("paramlist dcl") != string::npos) {
                                // Single dcl
                                auto dclNode = paramsNode->children[0];
                                for (auto dclChild : dclNode->children) {
                                    if (dclChild->rule.find("type") != string::npos) {
                                        string paramType = getType(dclChild);
                                        paramTypes.push_back(paramType);
                                        break;
                                    }
                                }
                            }
                        };
                        collectParams(child);
                        break;
                    }
                }
            }
            
            // Add to tables (this makes it available for subsequent procedures to call)
            tables[procName] = {paramTypes, std::map<string, string>()};
            
            // Now analyze this procedure
            if (!analyzeProcedure(node)) {
                return false;
            }
        }
        
        for (auto child : node->children) {
            if (!analyzeInOrder(child)) return false;
        }
        
        return true;
    };
    
    return analyzeInOrder(root);
}

// Function to annotate types in the parse tree
void annotateTypes(shared_ptr<TreeNode> root) {
    if (!root) return;
    
    // Annotate children first
    for (auto child : root->children) {
        annotateTypes(child);
    }
    
    // Annotate current node if it's an expression
    if (root->isExpression()) {
        if (root->tokenKind == "NUM") {
            root->type = "int";
        } else if (root->tokenKind == "NULL") {
            root->type = "int*";
        } else if (root->tokenKind == "ID") {
            // Don't overwrite types already set by setIdTypes()
            // and don't set default fallback types (they should be set correctly by setIdTypes)
        } else if (!root->isTerminal()) {
            // For nonterminal expressions, derive type from children
            string lhs = root->rule.substr(0, root->rule.find(' '));
            if (lhs == "expr" || lhs == "term" || lhs == "factor") {
                // Only set type if not already set by semantic analysis
                if (root->type.empty()) {
                // Find the first child that has a type
                for (auto child : root->children) {
                    if (!child->type.empty()) {
                        root->type = child->type;
                        break;
                    }
                }
                // If no child has type, default to int
                if (root->type.empty()) {
                    root->type = "int";
                    }
                }
            }
        }
    }
}

// Function to set ID types based on their declarations
void setIdTypes(shared_ptr<TreeNode> root) {
    if (!root) return;
    
    // Simple approach: traverse the tree and set types, with explicit handling for procedure names
    function<void(shared_ptr<TreeNode>, shared_ptr<TreeNode>)> traverse = [&](shared_ptr<TreeNode> node, shared_ptr<TreeNode> parent) {
        if (!node) return;
        
        if (node->tokenKind == "ID") {
            string name = node->lexeme;
            
            // First, check if this is a procedure name declaration and explicitly set empty type
            if (parent && !parent->isTerminal() && parent->rule.find("procedure INT ID") == 0) {
                if (parent->children.size() > 1 && parent->children[1].get() == node.get()) {
                    node->type = ""; // Explicitly ensure procedure names have no type
                    return;
                }
            }
            
            // Skip procedure calls (they shouldn't have types)
            if (parent && !parent->isTerminal() && parent->rule.find("factor ID LPAREN") == 0) {
                node->type = "";
                return;
            }
            
            // For variable declarations, set type from the declaration
            if (parent && !parent->isTerminal() && parent->rule.find("dcl type ID") != string::npos) {
                for (auto sibling : parent->children) {
                    if (!sibling->isTerminal() && sibling->rule.find("type") != string::npos) {
                        node->type = getType(sibling);
                        return;
                    }
                }
            }
            
            // For variable references, look up in current procedure's symbol table
            if (!currentProcedure.empty() && tables.find(currentProcedure) != tables.end()) {
                auto& symbolTable = tables[currentProcedure].second;
                if (symbolTable.find(name) != symbolTable.end()) {
                    node->type = symbolTable[name];
                }
            }
        }
        
        for (auto child : node->children) {
            traverse(child, node);
        }
    };
    
    // Traverse within each procedure context
    function<void(shared_ptr<TreeNode>)> traverseProcedures = [&](shared_ptr<TreeNode> node) {
        if (!node) return;
        
        if (!node->isTerminal() && 
            (node->rule.find("main INT WAIN") != string::npos ||
             node->rule.find("procedure INT ID") != string::npos)) {
            
            string oldProcedure = currentProcedure;
            if (node->rule.find("main INT WAIN") != string::npos) {
                currentProcedure = "wain";
            } else if (node->children.size() > 1) {
                currentProcedure = node->children[1]->lexeme;
            }
            
            traverse(node, nullptr);
            currentProcedure = oldProcedure;
        }
        
        for (auto child : node->children) {
            traverseProcedures(child);
        }
    };
    
    traverseProcedures(root);
}

// Function to output the type-annotated parse tree
void outputTree(shared_ptr<TreeNode> root) {
    if (!root) return;
    
    if (root->isTerminal()) {
        cout << root->tokenKind;
        if (!root->lexeme.empty()) {
            cout << " " << root->lexeme;
        }
        // Only print type if it's not empty
        if (!root->type.empty()) {
            cout << " : " << root->type;
        }
        cout << endl;
    } else {
        cout << root->rule;
        // Only print type for expr, term, factor, or lvalue nonterminals
        if (!root->type.empty() && !root->rule.empty()) {
            string lhs = root->rule.substr(0, root->rule.find(' '));
            if (lhs == "expr" || lhs == "term" || lhs == "factor" || lhs == "lvalue") {
            cout << " : " << root->type;
            }
        }
        cout << endl;
        
        for (auto child : root->children) {
            outputTree(child);
        }
    }
}

int main() {
    // rebuild parse tree
    shared_ptr<TreeNode> root = parseTree();
    if (!root) {
        cerr << "ERROR" << endl;
        return 1;
    }
    
    // semantic analysis
    if (!semanticAnalysis(root)) {
        cerr << "ERROR" << endl;
        return 1;
    }
    
    // add types
    annotateTypes(root);
    setIdTypes(root);  // Run this last to have final say on ID types
    
    // output the tree
    outputTree(root);
    
    return 0;
}

