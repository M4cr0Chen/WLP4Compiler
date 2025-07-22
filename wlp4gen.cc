#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cassert>
#include <memory>

using namespace std;

// Parse tree node structure
struct Node {
    string rule;        // Production rule or token kind
    string lexeme;      // Token lexeme (for terminals)
    string type;        // Type annotation (int or int*)
    vector<unique_ptr<Node>> children;
    
    Node() = default;
    
    // No need for explicit destructor - unique_ptr handles cleanup automatically
    
    // Check if this is a terminal node
    bool isTerminal() const {
        return children.empty();
    }
    
    // Get the token kind for terminals
    string getTokenKind() const {
        if (rule.find(' ') != string::npos) {
            return rule.substr(0, rule.find(' '));
        }
        return rule;
    }
};

// Symbol information
struct Symbol {
    string name;
    string type;
    int offset;  // Offset from frame pointer $29
};

// Procedure information  
struct Procedure {
    string name;
    vector<string> signature;  // Parameter types in order
    vector<string> paramNames;  // Parameter names in order
    map<string, Symbol> symbols;  // All variables in procedure
    int paramCount;
    int localCount;
    
    Procedure() : paramCount(0), localCount(0) {}
};

class CodeGenerator {
private:
    unique_ptr<Node> root;
    map<string, Procedure> procedures;
    string currentProc;
    int labelCounter;
    bool needsInit;  // Whether wain takes int* parameter
    
    // Helper functions for code generation
    void push(const string& reg) {
        cout << "sw " << reg << ", -4($30)" << endl;
        cout << "sub $30, $30, $4" << endl;
    }
    
    void pop(const string& reg) {
        cout << "add $30, $30, $4" << endl;
        cout << "lw " << reg << ", -4($30)" << endl;
    }
    
    string newLabel(const string& prefix) {
        return prefix + to_string(labelCounter++);
    }
    
    // Parse .wlp4ti input into parse tree
    unique_ptr<Node> parseInput() {
        string line;
        return parseNode();
    }
    
    unique_ptr<Node> parseNode() {
        string line;
        if (!getline(cin, line)) return nullptr;
        
        auto node = make_unique<Node>();
        
        // Check if this line has type annotation
        size_t colonPos = line.find(" : ");
        if (colonPos != string::npos) {
            string beforeColon = line.substr(0, colonPos);
            node->type = line.substr(colonPos + 3);
            line = beforeColon;
        }
        
        // Parse the rule or token
        size_t spacePos = line.find(' ');
        if (spacePos == string::npos) {
            // No space - single token rule
            node->rule = line;
        } else {
            string first = line.substr(0, spacePos);
            string rest = line.substr(spacePos + 1);
            
            // Check if this is a terminal (token kind + lexeme)
            bool isTerminal = (first == "BOF" || first == "EOF" || 
                              first == "NUM" || first == "ID" || first == "INT" ||
                              first == "WAIN" || first == "LPAREN" || first == "RPAREN" ||
                              first == "LBRACE" || first == "RBRACE" || first == "COMMA" ||
                              first == "SEMI" || first == "RETURN" || first == "BECOMES" ||
                              first == "PLUS" || first == "MINUS" || first == "STAR" ||
                              first == "SLASH" || first == "PCT" || first == "EQ" ||
                              first == "NE" || first == "LT" || first == "LE" ||
                              first == "GT" || first == "GE" || first == "IF" ||
                              first == "ELSE" || first == "WHILE" || first == "PRINTLN" ||
                              first == "PUTCHAR" || first == "GETCHAR" || first == "AMP" ||
                              first == "NULL" || first == "NEW" || first == "DELETE" ||
                              first == "LBRACK" || first == "RBRACK");
            
            if (isTerminal) {
                node->rule = first;
                node->lexeme = rest;
            } else {
                // Production rule
                node->rule = line;
                // Parse children based on RHS symbols
                istringstream iss(rest);
                string symbol;
                while (iss >> symbol) {
                    if (symbol != ".EMPTY") {
                        auto child = parseNode();
                        if (child) node->children.push_back(std::move(child));
                    }
                }
            }
        }
        
        return node;
    }
    
    // First pass: collect symbols and build procedure table
    void collectSymbols(const Node& node) {
        if (node.rule == "procedure INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
            collectProcedure(node);
        } else if (node.rule == "main INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
            collectWain(node);
        }
        
        for (const auto& child : node.children) {
            collectSymbols(*child);
        }
    }
    
    void collectProcedure(const Node& node) {
        Procedure proc;
        proc.name = node.children[1]->lexeme;  // ID
        currentProc = proc.name;
        
        // Collect parameters
        const Node& params = *node.children[3];
        collectParams(params, proc);
        
        // Collect local declarations
        const Node& dcls = *node.children[6];
        collectDecls(dcls, proc);
        
        // Fix parameter offsets
        fixParameterOffsets(proc);
        
        procedures[proc.name] = proc;
    }
    
    void collectWain(const Node& node) {
        Procedure proc;
        proc.name = "wain";
        currentProc = "wain";
        
        // Collect wain parameters
        const Node& dcl1 = *node.children[3];  // First parameter
        const Node& dcl2 = *node.children[5];  // Second parameter
        
        collectDcl(dcl1, proc, true);  // Parameter
        collectDcl(dcl2, proc, true);  // Parameter
        
        // Check if first parameter is int* (need init)
        needsInit = (dcl1.children[0]->rule == "type INT STAR");
        
        // Collect local declarations
        const Node& dcls = *node.children[8];
        collectDecls(dcls, proc);
        
        // Fix parameter offsets
        fixParameterOffsets(proc);
        
        procedures["wain"] = proc;
    }
    
    void collectParams(const Node& params, Procedure& proc) {
        if (params.rule == "params .EMPTY") return;
        
        // params -> paramlist
        const Node& paramlist = *params.children[0];
        collectParamlist(paramlist, proc);
    }
    
    void collectParamlist(const Node& paramlist, Procedure& proc) {
        if (paramlist.rule == "paramlist dcl") {
            collectDcl(*paramlist.children[0], proc, true);
        } else if (paramlist.rule == "paramlist dcl COMMA paramlist") {
            collectDcl(*paramlist.children[0], proc, true);
            collectParamlist(*paramlist.children[2], proc);
        }
    }
    
    void collectDecls(const Node& dcls, Procedure& proc) {
        if (dcls.rule == "dcls .EMPTY") return;
        
        if (dcls.rule == "dcls dcls dcl BECOMES NUM SEMI" ||
            dcls.rule == "dcls dcls dcl BECOMES NULL SEMI") {
            collectDecls(*dcls.children[0], proc);  // Recurse on nested dcls
            collectDcl(*dcls.children[1], proc, false);  // Collect this dcl
        }
    }
    
    void collectDcl(const Node& dcl, Procedure& proc, bool isParam) {
        Symbol sym;
        const Node& type = *dcl.children[0];
        const Node& id = *dcl.children[1];
        
        sym.name = id.lexeme;
        sym.type = (type.rule == "type INT") ? "int" : "int*";
        
        if (isParam) {
            // Parameters have positive offsets: first param gets highest offset
            sym.offset = 0;  // Temporary, will fix later
            proc.signature.push_back(sym.type);
            proc.paramNames.push_back(sym.name);
            proc.paramCount++;
        } else {
            // Local variables have non-positive offsets: -4*i where i=local variable index
            sym.offset = -4 * proc.localCount;
            proc.localCount++;
        }
        
        proc.symbols[sym.name] = sym;
    }
    
    void fixParameterOffsets(Procedure& proc) {
        // Assign offsets: first parameter gets highest offset 4*n, last gets 4
        for (int i = 0; i < proc.paramNames.size(); i++) {
            string paramName = proc.paramNames[i];
            proc.symbols[paramName].offset = 4 * (proc.paramCount - i);
        }
    }
    
    // Second pass: generate code
    void generateCode(const Node& node) {

        // cout << ".import init" << endl;
        // cout << ".import new" << endl;
        // cout << ".import delete" << endl;
        cout << ".import print" << endl;
        if (node.rule == "start BOF procedures EOF") {
            generateStart(node);
        }
    }
    
    void generateStart(const Node& node) {
        // Generate code for procedures, with wain first
        const Node& procedures = *node.children[1];
        generateProcedures(procedures);
    }
    
    void generateProcedures(const Node& procedures) {
        if (procedures.rule == "procedures main") {
            generateMain(*procedures.children[0]);
        } else if (procedures.rule == "procedures procedure procedures") {
            // Generate wain first, then other procedures
            generateProcedures(*procedures.children[1]);  // Find wain in the rest
            generateProcedure(*procedures.children[0]);   // Generate this procedure
        }
    }
    
    void generateMain(const Node& main) {
        currentProc = "wain";
        Procedure& proc = procedures["wain"];
        
        cout << "; wain procedure" << endl;
        generateWainPrologue(proc);
        
        // Generate dcls (local variable declarations)
        const Node& dcls = *main.children[8];
        generateDcls(dcls);
        
        // Generate statements
        const Node& statements = *main.children[9];
        generateStatements(statements);
        
        // Generate return expression
        const Node& expr = *main.children[11];
        generateExpr(expr);
        
        generateWainEpilogue(proc);
    }
    
    void generateProcedure(const Node& procedure) {
        string name = procedure.children[1]->lexeme;
        currentProc = name;
        Procedure& proc = procedures[name];
        
        cout << "; procedure " << name << endl;
        cout << "P" << name << ":" << endl;  // Prefix to avoid conflicts
        
        generateProcPrologue(proc);
        
        // Generate dcls (local variable declarations)
        const Node& dcls = *procedure.children[6];
        generateDcls(dcls);
        
        // Generate statements
        const Node& statements = *procedure.children[7];
        generateStatements(statements);
        
        // Generate return expression  
        const Node& expr = *procedure.children[9];
        generateExpr(expr);
        
        generateProcEpilogue(proc);
    }
    
    void generateWainPrologue(const Procedure& proc) {
        // Initialize constants
        cout << "; begin prologue" << endl;
        cout << "lis $4" << endl;
        cout << ".word 4" << endl;
        
        // Push parameters
        push("$1");  // First parameter
        push("$2");  // Second parameter
        
        // Set frame pointer
        cout << "sub $29, $30, $4" << endl;
        
        // Initialize local variables
        generateLocalInits(proc);
        
        // // Call init if needed
        // if (needsInit) {
        //     cout << "; call init" << endl;
        //     cout << ".import init" << endl;
        //     push("$31");
        //     cout << "add $2, $0, $2" << endl;  // Second parameter (array size)
        //     cout << "lis $5" << endl;
        //     cout << ".word init" << endl;
        //     cout << "jalr $5" << endl;
        //     pop("$31");
        // } else {
        //     cout << "; call init with 0" << endl;
        //     cout << ".import init" << endl;
        //     push("$31");
        //     cout << "add $2, $0, $0" << endl;  // 0 for non-array input
        //     cout << "lis $5" << endl;
        //     cout << ".word init" << endl;
        //     cout << "jalr $5" << endl;
        //     pop("$31");
        // }
        
        cout << "; end prologue" << endl;
    }
    
    void generateProcPrologue(const Procedure& proc) {
        cout << "; begin prologue" << endl;
        cout << "sub $29, $30, $4" << endl;  // Set frame pointer
        generateLocalInits(proc);
        cout << "; end prologue" << endl;
    }
    
    void generateLocalInits(const Procedure& proc) {
        // Local variables are already handled during dcls processing
        // This function is for explicit initialization from dcls rules
    }
    
    void generateDcls(const Node& dcls) {
        if (dcls.rule == "dcls .EMPTY") return;
        
        if (dcls.rule == "dcls dcls dcl BECOMES NUM SEMI") {
            generateDcls(*dcls.children[0]);  // Process nested dcls first
            
            // Initialize this variable with NUM value
            const Node& dcl = *dcls.children[1];
            const Node& num = *dcls.children[3];
            
            cout << "; initialize " << dcl.children[1]->lexeme << " = " << num.lexeme << endl;
            cout << "lis $5" << endl;
            cout << ".word " << num.lexeme << endl;
            cout << "sw $5, -4($30)" << endl;
            cout << "sub $30, $30, $4" << endl;
            
        } else if (dcls.rule == "dcls dcls dcl BECOMES NULL SEMI") {
            generateDcls(*dcls.children[0]);  // Process nested dcls first
            
            // Initialize this variable with NULL (1)
            const Node& dcl = *dcls.children[1];
            
            cout << "; initialize " << dcl.children[1]->lexeme << " = NULL" << endl;
            cout << "lis $5" << endl;
            cout << ".word 1" << endl;  // NULL is 1
            cout << "sw $5, -4($30)" << endl;
            cout << "sub $30, $30, $4" << endl;
        }
    }
    
    void generateWainEpilogue(const Procedure& proc) {
        cout << "; begin epilogue" << endl;
        // Pop local variables and parameters
        int totalVars = proc.paramCount + proc.localCount;
        for (int i = 0; i < totalVars; i++) {
            cout << "add $30, $30, $4" << endl;
        }
        cout << "jr $31" << endl;
    }
    
    void generateProcEpilogue(const Procedure& proc) {
        cout << "; begin epilogue" << endl;
        // Pop only local variables (caller pops parameters)
        for (int i = 0; i < proc.localCount; i++) {
            cout << "add $30, $30, $4" << endl;
        }
        cout << "jr $31" << endl;
    }
    
    void generateStatements(const Node& statements) {
        if (statements.rule == "statements .EMPTY") return;
        
        if (statements.rule == "statements statements statement") {
            generateStatements(*statements.children[0]);
            generateStatement(*statements.children[1]);
        }
    }
    
    void generateStatement(const Node& statement) {
        if (statement.rule == "statement lvalue BECOMES expr SEMI") {
            generateAssignment(statement);
        } else if (statement.rule == "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE") {
            generateIf(statement);
        } else if (statement.rule == "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE") {
            generateWhile(statement);
        } else if (statement.rule == "statement PRINTLN LPAREN expr RPAREN SEMI") {
            generatePrintln(statement);
        } else if (statement.rule == "statement PUTCHAR LPAREN expr RPAREN SEMI") {
            generatePutchar(statement);
        } else if (statement.rule == "statement DELETE LBRACK RBRACK expr SEMI") {
            generateDelete(statement);
        }
    }
    
    void generateLvalueAssignment(const Node& lvalue, const Node& expr) {
        if (lvalue.rule == "lvalue ID") {
            string varName = lvalue.children[0]->lexeme;
            
            // Special case: wain parameters go directly to registers
            if (currentProc == "wain") {
                Procedure& proc = procedures[currentProc];
                if (proc.paramNames.size() >= 1 && varName == proc.paramNames[0]) {
                    // First parameter -> $1
                    generateExpr(expr);
                    cout << "add $1, $3, $0" << endl;
                    Symbol& sym = procedures[currentProc].symbols[varName];
                    cout << "sw $3, " << sym.offset << "($29)" << endl;
                    return;
                } else if (proc.paramNames.size() >= 2 && varName == proc.paramNames[1]) {
                    // Second parameter -> $2  
                    generateExpr(expr);
                    cout << "add $2, $3, $0" << endl;
                    Symbol& sym = procedures[currentProc].symbols[varName];
                    cout << "sw $3, " << sym.offset << "($29)" << endl;
                    return;
                }
            }
            
            // Regular case: stack variable assignment
            generateExpr(expr);
            Symbol& sym = procedures[currentProc].symbols[varName];
            cout << "sw $3, " << sym.offset << "($29)" << endl;
        } else if (lvalue.rule == "lvalue STAR factor") {
            // Pointer dereference assignment
            generateExpr(expr);
            push("$3");  // Save expr value
            generateFactor(*lvalue.children[1]);  // Get address
            pop("$5");   // Restore expr value
            cout << "sw $5, 0($3)" << endl;  // Store at address
        } else if (lvalue.rule == "lvalue LPAREN lvalue RPAREN") {
            // Recursively handle the inner lvalue
            generateLvalueAssignment(*lvalue.children[1], expr);
        }
    }
    
    void generateAssignment(const Node& statement) {
        const Node& lvalue = *statement.children[0];
        const Node& expr = *statement.children[2];
        generateLvalueAssignment(lvalue, expr);
    }
    
    void generateIf(const Node& statement) {
        string elseLabel = newLabel("else");
        string endLabel = newLabel("endif");
        
        const Node& test = *statement.children[2];
        const Node& thenStmts = *statement.children[5];
        const Node& elseStmts = *statement.children[9];
        
        generateTest(test, elseLabel);  // Jump to else if test fails
        generateStatements(thenStmts);
        cout << "beq $0, $0, " << endLabel << endl;  // Jump to end
        cout << elseLabel << ":" << endl;
        generateStatements(elseStmts);
        cout << endLabel << ":" << endl;
    }
    
    void generateWhile(const Node& statement) {
        string startLabel = newLabel("while");
        string endLabel = newLabel("endwhile");
        
        const Node& test = *statement.children[2];
        const Node& stmts = *statement.children[5];
        
        cout << startLabel << ":" << endl;
        generateTest(test, endLabel);  // Jump to end if test fails
        generateStatements(stmts);
        cout << "beq $0, $0, " << startLabel << endl;  // Jump back to start
        cout << endLabel << ":" << endl;
    }
    
    void generateTest(const Node& test, const string& failLabel) {
        const Node& expr1 = *test.children[0];
        const Node& op = *test.children[1];
        const Node& expr2 = *test.children[2];
        
        generateExpr(expr1);
        push("$3");
        generateExpr(expr2);
        pop("$5");  // expr1 in $5, expr2 in $3
        
        string opToken = op.rule;
        bool isPointer = (expr1.type == "int*" || expr2.type == "int*");
        
        if (opToken == "EQ") {
            cout << "bne $5, $3, " << failLabel << endl;
        } else if (opToken == "NE") {
            cout << "beq $5, $3, " << failLabel << endl;
        } else if (opToken == "LT") {
            if (isPointer) {
                cout << "sltu $6, $5, $3" << endl;
            } else {
                cout << "slt $6, $5, $3" << endl;
            }
            cout << "beq $6, $0, " << failLabel << endl;
        } else if (opToken == "LE") {
            if (isPointer) {
                cout << "sltu $6, $3, $5" << endl;
            } else {
                cout << "slt $6, $3, $5" << endl;
            }
            cout << "bne $6, $0, " << failLabel << endl;
        } else if (opToken == "GT") {
            if (isPointer) {
                cout << "sltu $6, $3, $5" << endl;
            } else {
                cout << "slt $6, $3, $5" << endl;
            }
            cout << "beq $6, $0, " << failLabel << endl;
        } else if (opToken == "GE") {
            if (isPointer) {
                cout << "sltu $6, $5, $3" << endl;
            } else {
                cout << "slt $6, $5, $3" << endl;
            }
            cout << "bne $6, $0, " << failLabel << endl;
        }
    }
    
    void generatePrintln(const Node& statement) {
        const Node& expr = *statement.children[2];
        
        push("$1");
        push("$31");
        generateExpr(expr);
        cout << "add $1, $3, $0" << endl;
        cout << "lis $5" << endl;
        cout << ".word print" << endl;
        cout << "jalr $5" << endl;
        pop("$31");
        pop("$1");
    }
    
    void generatePutchar(const Node& statement) {
        const Node& expr = *statement.children[2];
        generateExpr(expr);
        cout << "lis $5" << endl;
        cout << ".word 0xffff000c" << endl;
        cout << "sw $3, 0($5)" << endl;
    }
    
    void generateDelete(const Node& statement) {
        const Node& expr = *statement.children[3];
        
        push("$1");
        push("$31");
        generateExpr(expr);
        
        // Check if expr is NULL (1), if so, do nothing
        string skipLabel = newLabel("skipdelete");
        cout << "lis $5" << endl;
        cout << ".word 1" << endl;
        cout << "beq $3, $5, " << skipLabel << endl;
        
        cout << "add $1, $3, $0" << endl;
        cout << "lis $5" << endl;
        cout << ".word delete" << endl;
        cout << "jalr $5" << endl;
        
        cout << skipLabel << ":" << endl;
        pop("$31");
        pop("$1");
    }
    
    void generateExpr(const Node& expr) {
        if (expr.rule == "expr term") {
            generateTerm(*expr.children[0]);
        } else if (expr.rule == "expr expr PLUS term") {
            generateExprBinaryOp(*expr.children[0], *expr.children[2], "PLUS", expr.type);
        } else if (expr.rule == "expr expr MINUS term") {
            generateExprBinaryOp(*expr.children[0], *expr.children[2], "MINUS", expr.type);
        }
    }
    
    void generateTerm(const Node& term) {
        if (term.rule == "term factor") {
            generateFactor(*term.children[0]);
        } else if (term.rule == "term term STAR factor") {
            generateTermBinaryOp(*term.children[0], *term.children[2], "STAR");
        } else if (term.rule == "term term SLASH factor") {
            generateTermBinaryOp(*term.children[0], *term.children[2], "SLASH");
        } else if (term.rule == "term term PCT factor") {
            generateTermBinaryOp(*term.children[0], *term.children[2], "PCT");
        }
    }
    
    void generateExprBinaryOp(const Node& left, const Node& right, const string& op, const string& resultType) {
        // For expr-level operations: left is expr, right is term
        if (op == "PLUS") {
            if (left.type == "int*" && right.type == "int") {
                // Pointer + int
                generateExpr(left);
                push("$3");
                generateTerm(right);
                cout << "mult $3, $4" << endl;
                cout << "mflo $3" << endl;
                pop("$5");
                cout << "add $3, $5, $3" << endl;
            } else if (left.type == "int" && right.type == "int*") {
                // int + Pointer
                generateExpr(left);
                cout << "mult $3, $4" << endl;
                cout << "mflo $3" << endl;
                push("$3");
                generateTerm(right);
                pop("$5");
                cout << "add $3, $5, $3" << endl;
            } else {
                // int + int
                generateExpr(left);
                push("$3");
                generateTerm(right);
                pop("$5");
                cout << "add $3, $5, $3" << endl;
            }
        } else if (op == "MINUS") {
            if (left.type == "int*" && right.type == "int") {
                // Pointer - int
                generateExpr(left);
                push("$3");
                generateTerm(right);
                cout << "mult $3, $4" << endl;
                cout << "mflo $3" << endl;
                pop("$5");
                cout << "sub $3, $5, $3" << endl;
            } else if (left.type == "int*" && right.type == "int*") {
                // Pointer - Pointer
                generateExpr(left);
                push("$3");
                generateTerm(right);
                pop("$5");
                cout << "sub $3, $5, $3" << endl;
                cout << "div $3, $4" << endl;
                cout << "mflo $3" << endl;
            } else {
                // int - int
                generateExpr(left);
                push("$3");
                generateTerm(right);
                pop("$5");
                cout << "sub $3, $5, $3" << endl;
            }
        }
    }
    
    void generateTermBinaryOp(const Node& left, const Node& right, const string& op) {
        // For term-level operations: left is term, right is factor
        if (op == "STAR") {
            // Multiplication
            generateTerm(left);
            push("$3");
            generateFactor(right);
            pop("$5");
            cout << "mult $5, $3" << endl;
            cout << "mflo $3" << endl;
        } else if (op == "SLASH") {
            // Division
            generateTerm(left);
            push("$3");
            generateFactor(right);
            pop("$5");
            cout << "div $5, $3" << endl;
            cout << "mflo $3" << endl;
        } else if (op == "PCT") {
            // Modulo
            generateTerm(left);
            push("$3");
            generateFactor(right);
            pop("$5");
            cout << "div $5, $3" << endl;
            cout << "mfhi $3" << endl;
        }
    }
    
    void generateFactor(const Node& factor) {
        if (factor.rule == "factor NUM") {
            cout << "lis $3" << endl;
            cout << ".word " << factor.children[0]->lexeme << endl;
        } else if (factor.rule == "factor NULL") {
            cout << "lis $3" << endl;
            cout << ".word 1" << endl;  // NULL is 1
        } else if (factor.rule == "factor ID") {
            string varName = factor.children[0]->lexeme;
            Symbol& sym = procedures[currentProc].symbols[varName];
            cout << "lw $3, " << sym.offset << "($29)" << endl;
        } else if (factor.rule == "factor LPAREN expr RPAREN") {
            generateExpr(*factor.children[1]);
        } else if (factor.rule == "factor AMP lvalue") {
            generateAddressOf(*factor.children[1]);
        } else if (factor.rule == "factor STAR factor") {
            generateFactor(*factor.children[1]);
            cout << "lw $3, 0($3)" << endl;
        } else if (factor.rule == "factor NEW INT LBRACK expr RBRACK") {
            generateNew(factor);
        } else if (factor.rule == "factor GETCHAR LPAREN RPAREN") {
            cout << "lis $5" << endl;
            cout << ".word 0xffff0004" << endl;
            cout << "lw $3, 0($5)" << endl;
        } else if (factor.rule == "factor ID LPAREN RPAREN") {
            generateProcCall(factor.children[0]->lexeme, nullptr);
        } else if (factor.rule == "factor ID LPAREN arglist RPAREN") {
            generateProcCall(factor.children[0]->lexeme, factor.children[2].get());
        }
    }
    
    void generateAddressOf(const Node& lvalue) {
        if (lvalue.rule == "lvalue ID") {
            string varName = lvalue.children[0]->lexeme;
            Symbol& sym = procedures[currentProc].symbols[varName];
            cout << "lis $3" << endl;
            cout << ".word " << sym.offset << endl;
            cout << "add $3, $29, $3" << endl;
        } else if (lvalue.rule == "lvalue STAR factor") {
            generateFactor(*lvalue.children[1]);
        } else if (lvalue.rule == "lvalue LPAREN lvalue RPAREN") {
            generateAddressOf(*lvalue.children[1]);
        }
    }
    
    void generateNew(const Node& factor) {
        const Node& expr = *factor.children[3];
        
        push("$1");
        push("$31");
        generateExpr(expr);
        cout << "add $1, $3, $0" << endl;
        cout << "lis $5" << endl;
        cout << ".word new" << endl;
        cout << "jalr $5" << endl;
        
        // Check if allocation failed (returned 0), convert to NULL (1)
        string successLabel = newLabel("allocsuccess");
        cout << "bne $3, $0, " << successLabel << endl;
        cout << "lis $3" << endl;
        cout << ".word 1" << endl;
        cout << successLabel << ":" << endl;
        
        pop("$31");
        pop("$1");
    }
    
    void generateProcCall(const string& procName, const Node* arglist) {
        push("$29");
        push("$31");
        
        // Count and push arguments
        int argCount = 0;
        if (arglist) {
            argCount = pushArguments(*arglist);
        }
        
        // Make the call
        cout << "lis $5" << endl;
        cout << ".word P" << procName << endl;
        cout << "jalr $5" << endl;
        
        // Pop arguments
        for (int i = 0; i < argCount; i++) {
            cout << "add $30, $30, $4" << endl;
        }
        
        pop("$31");
        pop("$29");
    }
    
    int pushArguments(const Node& arglist) {
        if (arglist.rule == "arglist expr") {
            generateExpr(*arglist.children[0]);
            push("$3");
            return 1;
        } else if (arglist.rule == "arglist expr COMMA arglist") {
            generateExpr(*arglist.children[0]);
            push("$3");
            return 1 + pushArguments(*arglist.children[2]);
        }
        return 0;
    }

public:
    CodeGenerator() : labelCounter(1), needsInit(false) {}
    
    // No need for explicit destructor - unique_ptr handles cleanup automatically
    
    void run() {
        // Parse input
        root = parseInput();
        if (!root) return;
        
        // First pass: collect symbols
        collectSymbols(*root);

        // cout << "outputting symbols" << endl;
        // cout << procedures.size() << " procedures found." << endl;
        // for (auto & proc : procedures) {
        //     cout << "; Procedure: " << proc.first << endl;
        //     for (auto & sym : proc.second.symbols) {
        //         cout << "; Symbol: " << sym.second.name << ", Type: " << sym.second.type 
        //              << ", Offset: " << sym.second.offset << endl;
        //     }
        // }
        
        // Second pass: generate code
        generateCode(*root);
    }
};

int main() {
    CodeGenerator generator;
    generator.run();
    return 0;
}
