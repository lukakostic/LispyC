
#include"../simpLex.hpp"

#include<optional>
// #include<unordered_map>
// #include<vector>
#include<fstream>
#include<sstream>
// #include<iostream>
#include <stdint.h>
// using uint8_t
// using uint32_t = u_int32_t;
// using uint16_t = u_int16_t;

//debug and runtime asserts
#define DBG false
#define ASS true

using namespace std;
using namespace SimpLex;

std::string readFileIntoString(std::string filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    
    if (file) {
        buffer << file.rdbuf();
    } else {
        // Handle error - for example, throw an exception or return a special value
    }

    return buffer.str();
}


//  eg: (true, "{","}",".")
#define ___pushSymbols(splitting,...) do{                                                    \
        const char *ARRAY[] = {__VA_ARGS__};                                         \
        for (uint32_t II = 0; II < sizeof(ARRAY) / sizeof(char *); II++)                  \
            l.registerNewSymbol(II, string(ARRAY[II]),splitting); \
    }while(0);
// eg: ("new","free","for","while")
#define ___pushIdents(...) do{                                      \
        const char *ARRAY[] = {__VA_ARGS__};                        \
        for (uint32_t II = 0; II < sizeof(ARRAY) / sizeof(char *); II++) \
            l.regIdent(string(ARRAY[II]));                     \
    }while(0);
 //idents.push_back(string(ARRAY[II]));

void __populateExampleTokens(Lex& l)
{ // some example tokens
    //___pushIdents("new", "free", "delete", "for", "while", "return", "break", "continue", "else", "elif", "try", "finally", "switch", "case");

    ___pushSymbols( true,
                    "{", "}", "(", ")", "[", "]",
                    // "(("", "))",
                    
                    // "!(" ,"!{", "^(","^{", // lazy eval
                    // "({","})", //dictionary / any-obj

                    // ".","...", ",", ":", "::", ";", "?", 
    );

    ___pushIdents("", "TRUE","FALSE","NOP","LIST","PRINT","EVAL","ALIAS","=","+","-","*","==","!=","LET","DYN","DYNST","FNDECL");//":",".",);
}
static constexpr enum INTERNAL :uint8_t {
    _EMPTY_, TRUE,FALSE,NOP,LIST,PRINT,EVAL,ALIAS,eq,plus,minus,asterisk,dblEq,notDblEq,LET,DYN,DYNST,FNDECL
} INTERNAL = {};
/*
struct Inst {
    enum class Type : uint8_t{
        FnCall
    } type;
    
    #define PK __attribute__((packed))
    struct PK FnCall{
        uint32_t fnName_lexStringIdx;
        uint8_t n_args;
    };
    #undef PK
};
*/

#define ERR(msg) {msg;cout.flush();abort();}

/* Assert no overflows X-Y */
template<typename T, typename T2> 
inline auto subO( T x, T2 minus ){
    auto r = x - minus;
    if(ASS && r>x) [[unlikely]] 
        ERR(cout<<"Overflow.");
    return r;
}

// template<typename idx_type, idx_type NULL_VAL, typename containerType, typename valueType>
#define IDX(name,idx_type,null_val,containerType,valueType) struct name {static containerType* container;\
idx_type i;valueType operator() (){return (*container)[i];} };
// IDX(stackIdx, uint32_t, UINT32_MAX, std::vector<char> , char );



struct Compiler
{
    static Compiler* instance;  

    struct Options {
        bool redeclareInSameScope;
        bool redeclareKeywords;
    } opts;

    enum class ValueType : uint8_t; //predefine
    
    struct Value
    {
        ValueType type;
        uint32_t index; // index in some array, or in executionStack
    };

    // struct AnyHolders{ multiple arrays, for each ValueType. Used for GC. };
    struct Any
    {
        ValueType type;
        void* ptr; // pointer to heap
        uint32_t refCount;
    };
    struct AnyStatic
    {
        ValueType type;
        union {
            double Variable_Number;
            string Variable_String;
            vector<Value> Variable_List;
            Any Variable_Object;
            //[TODO] other types
        } value;
    };

    enum class ValueType : uint8_t {
        UNINITIALIZED,

        Variable_Number,
        Variable_String,
        Variable_List,
        Any_Dynamic,
        Any_Static,
        // Variable_Object,

        Function,
        Macro,
        
        Internal_Function,      // PRINT , LIST , NOP
        Internal_Value_Number,  // __NUM_ARGS__ , TRUE , FALSE
        // Internal_Macro,

        Identifier,  // for declarations/assignments (fn or variable) just symbolic name
    };
    static constexpr uint8_t ValueType_Size[]= {
        0,//UNINITIALIZED,

        sizeof(double),//Variable_Number,
        sizeof(string),//Variable_String,
        sizeof(vector<Value>),//Variable_List,
        // sizeof(Any),//Variable_Object,
        sizeof(Any),//Any_Dynamic,
        sizeof(AnyStatic),//Any_Static,
        
        
        
        0,//Function,
        0,//Macro,
        
        0,//Internal_Function,      // PRINT , LIST , NOP
        0,//Internal_Value_Number,  // __NUM_ARGS__ , TRUE , FALSE
        0,// Internal_Macro,

        0,//Identifier,  // for declarations/assignments (fn or variable) just symbolic name
    };
    static constexpr uint8_t ValueType_Alignment[] = {
        0,//UNINITIALIZED,

        alignment_of< double >(),//Variable_Number,
        alignment_of< string >(),//Variable_String,
        alignment_of< vector<Value> >(),//Variable_List,
        // alignment_of< Any >(),//Variable_Object,
        alignment_of< Any >(),//Any_Dynamic,
        alignment_of< AnyStatic >(),//Any_Static,
        
        0,//Function,
        0,//Macro,
        
        0,//Internal_Function,      // PRINT , LIST , NOP
        0,//Internal_Value_Number,  // __NUM_ARGS__ , TRUE , FALSE
        0,// Internal_Macro,

        0,//Identifier,  // for declarations/assignments (fn or variable) just symbolic name

    };
    struct Variable
    {
        static const uint32_t NO_NAME = UINT32_MAX;
        uint32_t nameString;
        Value v;
        uint32_t scopeIdx; //scope which it is tied to.
        Variable(Value _v,uint32_t _scopeIdx):nameString(NO_NAME),v(_v),scopeIdx(_scopeIdx){}
        Variable(Value _v,uint32_t ns,uint32_t _scopeIdx):nameString(ns),v(_v),scopeIdx(_scopeIdx){}
        bool isAnonymous(){return (nameString==NO_NAME);}
    };
    struct Scope
    {
        // uint32_t Variables_startIdx;
        // uint16_t Variables_count;
        uint32_t values_startIdx;
        // uint16_t values_count;  // ne treba mi values_count jer samo top level scope se izvrsava. Plus moze se izracunati tako sto oduzmem od startIdx scope iznad mene.
        uint32_t execStack_startIdx;
        bool transparentVariables,transparentValues;  //-> ako transparent nema Variables, vec ih prosledjuje na scope iznad
        Scope(uint32_t valuesStart, uint32_t stackStart, bool _transparentBind,bool _transparentVal)
        :values_startIdx(valuesStart),execStack_startIdx(stackStart),transparentVariables(_transparentBind),transparentValues(_transparentVal)
        {}
        Scope(bool _transparentBind,bool _transparentVal)
        :transparentVariables(_transparentBind),transparentValues(_transparentVal)
        {
            auto c = Compiler::instance;
            values_startIdx = c->valuesStack.size();
            execStack_startIdx = c->executionStack.stack.size();
        }
        
        // {
        //     // Variables_startIdx = VariablesStart;
        //     // Variables_count = 0;
        //     // values_startIdx = valuesStart;

        //     // transparent = _transparent;
        //     // values_count = 0;
        //     // total_bytes = 0;
        // }
        optional<Value> values(uint32_t idx){
            // if(idx>=values_count) return nullopt;
            return Compiler::instance->valuesStack[values_startIdx+idx];
        }
        uint32_t valuesCount(uint32_t thisStackIndex){
            auto&& ss = Compiler::instance->scopesStack;
            auto&& vs = Compiler::instance->valuesStack;
            ++thisStackIndex; //we need one after us to compare value_startIdx and get diff
            if(ss.size() == thisStackIndex) //are we top level?
                return vs.size() - values_startIdx; //all values to top are ours
            return ss[thisStackIndex].values_startIdx - values_startIdx; // diff next with our
        } 
        // optional<Variable> Variables(uint32_t idx){
        //     if(idx>=Variables_count) return nullopt;
        //     return Compiler::instance->variablesStack[Variables_startIdx+idx];
        // } 
        bool set_values(uint32_t idx,Value v){
            if(ASS){ //[TODO] check if scopes above me 
                // if()
            }
            // if(idx>=values_count) return false;
            Compiler::instance->valuesStack[values_startIdx+idx] = v;
            return true;
        } 
        // bool set_Variables(uint32_t idx,Variable b){
        //     if(idx>=Variables_count) return false;
        //     Compiler::instance->variablesStack[Variables_startIdx+idx] = b;
        //     return true;
        // } 
    };

    struct ScopeDetails{
        /* Optional scope details.*/
        struct DetailSet
        {
            uint32_t scopeIdx; //index in scopeStack
            unordered_map<string,string> details;
        };
        vector<DetailSet> scopeDetails;
        void ScopePopped(uint32_t scopeIdx){
            for(long i = scopeDetails.size()-1l; i>=0l; i--){
                if(scopeDetails[i].scopeIdx>=scopeIdx){ // should be == , > is impossible.. but oh well lets handle it
                    scopeDetails.pop_back();
                    continue;
                }else{
                    break;
                }
            }
        }
        void Set(uint32_t scopeIdx, string key, string value){
            if(scopeDetails.size()>0 && scopeDetails.back().scopeIdx == scopeIdx){
                //scope details exist, set data
                scopeDetails.back().details[key] = value;
                return;
            }
            
            //no details for scope yet, create entry first
            scopeDetails.push_back({scopeIdx,{}});
            Set(scopeIdx,key,value); //then set it
        }
        optional<string> Get(uint32_t scopeIdx, string key){
            if(scopeDetails.size()>0 && scopeDetails.back().scopeIdx == scopeIdx){
                //get data
                auto&& s = scopeDetails.back();
                auto f = s.details.find(key);
                if(f != s.details.end())
                    return f->second; //return value
            }
            return nullopt;
        }
    } details;

    using stackIdx = uint32_t;
    struct RuntimeStack {

        vector<std::byte> stack;
        // stackIdx sp; //stack pointer
        // vector<std::byte> instructions;
        // stackIdx ip; //instruction pointer

        RuntimeStack(){
            // sp = 0;
            stack.reserve(1024);
            // ip = 0;
            // instructions.reserve(1024);
        }

        inline void* stackPtr(uint32_t offset){
            return stack.data() + offset;
        }
        stackIdx allocBytes( uint16_t count, uint8_t alignment ){
            stackIdx beginningIdx = stack.size();

            {//insert padding before to align
                uint8_t neededPad = alignment-(beginningIdx % alignment); //8-(7%8) = 8-7 = 1
                count+=neededPad;
                beginningIdx+=neededPad;
                // if(insertedPadding_out!=nullptr) *insertedPadding_out += neededPad;
            }

            //if(DBG) cout<<"ALLOC:"<<beginningIdx<<" "<<(int)count<<endl;cout.flush();
            for(uint16_t i = 0; i<count;i++)
                stack.push_back({});
            
            // sp += count;
            return beginningIdx;// stack.data() + beginningIdx; 
        }
        stackIdx allocBytes( ValueType type ){
            return allocBytes( ValueType_Size[(uint8_t)type],ValueType_Alignment[(uint8_t)type]); 
        }
        void clearBytesTo(const uint32_t endIdx){
            // while(uint16_t i = 0; i<count;i++){
            //     stack.pop_back();
            // }
            if(stack.size()>endIdx)
                stack.resize(endIdx);
        }
        /*
        // void* __pushBytes_instructions(const uint8_t count){
        //     const stackIdx endIdx = stack.size();
        //     for(int i = 0; i<count;i++){
        //         stack.push_back({});
        //     }
        //     return stack.data() + endIdx; 
        // }
        //pi = push instruction
        //ex = execute instruction
        // void pi_fnCall(uint32_t stringIdx, uint8_t n_args ){
        //     *((Inst::FnCall*)(__pushBytes_instructions((32+8)/8))) = Inst::FnCall{stringIdx,n_args};
        // }
        // void ex_fnCall(){
        //     Inst::FnCall p = *(Inst::FnCall*)(instructions.data()+sp);
        */  
        stackIdx allocNumber(double num){
            stackIdx idx = allocBytes(ValueType::Variable_Number );
            *((double*)stackPtr(idx)) = num;
            return idx;
        }
        stackIdx allocNumber(string str){
            return allocNumber(stod(str));
        }
        stackIdx allocString(string str){
            stackIdx idx = allocBytes(ValueType::Variable_String );
            new((string*)stackPtr(idx)) string(str); //placement new
            return idx;
        }
        double readNumber(uint32_t stackIdx){
            return *((double*)stackPtr(stackIdx) );
        }
        string readString(uint32_t stackIdx){
            return *((string*)stackPtr(stackIdx) );
        }
        
    };
    
    
    Lex lex;
    uint32_t curLexToken;

    RuntimeStack executionStack;
    vector<Value> valuesStack; //values in transit
    vector<Variable> variablesStack; // bound to a name or anonymous, but not in transit

    vector<Scope> scopesStack;
    
    // inline Scope& scope(){return (*(scopesStack.end()-1));}
    /* return index of top scope */
    inline uint32_t scopeIdx(){ return subO(scopesStack.size(),1u); }
    /* return scope reference at index */
    inline Scope& scope(){ return scopesStack[scopeIdx()]; }
    inline Scope& scope(uint32_t i){ return scopesStack[i]; }
    string nameToStr(uint32_t nameIdx){
        return lex.stringStorage_entries[nameIdx].to_string(lex.stringStorage);
    }
    void pushValue(Value v){
        // auto&& s = scope();
        // s.values_count++;
        // s.total_bytes+=ValueType_Size[(uint8_t)v.type];
        valuesStack.push_back(v);
    }
    // void popValue(){
    //     Value& v = valuesStack.back();
    //     (*(scopesStack.end()-1)).values_count--;
    //     { //call destructors of underlying storage types
    //         if(v.type == ValueType::Variable_List){
    //             (((vector<Value>*)executionStack.stackPtr()+v.index))->~vector();
    //         }else if(v.type == ValueType::Variable_String){
    //             (((string*)executionStack.stackPtr()+v.index))->~string();
    //         }
    //     }
    //     executionStack.__popBytes( Value::stackSizes[(uint64_t)v.type] );
    //     // Value* v = (valuesStack.end()-1).base();
    //     valuesStack.pop_back();
    // }
    // void pushVariable(Value n){
    //     //(*(scopesStack.end()-1)).Variables_count++;
    //     variablesStack.push_back(Variable(n,topNonTransparentStack()));
    // }
    uint32_t topNonTransparentStack(){
        uint32_t stackIdx = subO(scopesStack.size(),1u);
        while(stackIdx>0 && scopesStack[stackIdx].transparentVariables)
            stackIdx--;
        return stackIdx;
    }
    void pushVariable(Value n,uint32_t nameString){
        // scope().Variables_count++;
        variablesStack.push_back(Variable(n,nameString,topNonTransparentStack()));
    }
    // void popVariable(){
    //     // scope().Variables_count--;
        
    //     variablesStack.pop_back();
    // }
    inline void pushScope(bool transparentVariables, bool transparentValues){
        scopesStack.push_back(Scope(transparentVariables,transparentValues));
        //valuesStack.size(),executionStack.stack.size(),
    }
    void popScope(){
        uint32_t si = scopeIdx();
        auto&& s = scope(si);
        uint32_t values_count = s.valuesCount(si);
        // 
        // popVariables();
        if(s.transparentVariables == false){
            for(long i = ((long)variablesStack.size())-1l; i>=0l;i--){
                if(variablesStack[i].scopeIdx==si)
                    variablesStack.pop_back();
            }
        }
        
        
        if(values_count>0){
            for(long i = ((long)values_count)-1; i>=0;i--){
                Value& v = valuesStack[s.values_startIdx+uint32_t(i)];
                { //call destructors of underlying storage types
                    if(v.type == ValueType::Variable_List){
                        ((vector<Value>*)executionStack.stackPtr(v.index))->~vector();
                    }else if(v.type == ValueType::Variable_String){
                        ((string*)executionStack.stackPtr(v.index))->~string();
                    }
                }
                // executionStack.__popBytes( Value::stackSizes[(uint64_t)v.type] );
                // Value* v = (valuesStack.end()-1).base();
                // valuesStack.pop_back();
            }
            valuesStack.resize(valuesStack.size()-values_count);
        }
        executionStack.clearBytesTo( s.execStack_startIdx );
        scopesStack.pop_back();
    }
    long ResolveIdentifierVariable(uint32_t nameIdx){
        if(DBG) cout << "Resolve identifier Variable: "<<nameToStr(nameIdx)<<endl;
        for(int i = variablesStack.size()-1;i>=0;i--){
            if(variablesStack[i].nameString == nameIdx){
                if(DBG) cout << "Resolved index: "<<i<<endl;
                return i;
            }
        }
        return -1;
    }
    Value ResolveIdentifierValue(uint32_t nameIdx){
        if(lex.stringStorage.allStrings[lex.stringStorage_entries[nameIdx].start] == '!'){
            //name starts with !

            string s = nameToStr(nameIdx);
            s = s.substr(1); //remove !
            
            nameIdx = lex.regIdent(s); // find index of name without ! (or register it)
            return {ValueType::Identifier,nameIdx};
        }
        //else
        long i = ResolveIdentifierVariable(nameIdx);
        if(i==-1) [[unlikely]]
            ERR(cout<<"Name "<<nameToStr(nameIdx)<<" not found.");
        
        if(variablesStack[i].v.type == ValueType::Internal_Value_Number){
            switch (variablesStack[i].v.index){
            case INTERNAL::TRUE :
                return Value{ValueType::Variable_Number, executionStack.allocNumber(1)};
            case INTERNAL::FALSE :
                return Value{ValueType::Variable_Number, executionStack.allocNumber(0)};
            }
        }
        return (variablesStack[i].v);
    
        return Value{ValueType::UNINITIALIZED,0};
    }
    string ValueToString(Value v,bool quoteStrings=false){
        switch (v.type)
        {
        case ValueType::Identifier :
            return string("[Identifier '") + nameToStr(v.index) +string("']");
        case ValueType::Function :
            return string("[Function]");
        case ValueType::Internal_Function :
            return string("[InternalFunction]");
        case ValueType::Variable_String :
            if(quoteStrings==false) return executionStack.readString(v.index);
            return string("\"") + executionStack.readString(v.index) +string("\"");
        case ValueType::Variable_Number :{
            string s = std::to_string(executionStack.readNumber(v.index));
            uint8_t end = (uint8_t)s.size();
            while(s[--end]=='0');
            if(s[end]=='.')end--;
            return s.substr(0,end+1);
        }default:
            return string("[Unknown]");
        }   
    }
    void ExecuteFnScopeAndPop(){ // at end of () or [] , we need to read first value and execute upon it.
        Value v = valuesStack[scope().values_startIdx];
        if(v.type != ValueType::Identifier){
            ERR(cout<<"Expected identifier name as first in (). Inline functions not yet supported."<<endl);
        }else{
            uint32_t fnName = v.index;
            Value fn = ResolveIdentifierValue(fnName);
            
            auto si = scopeIdx();
            auto&& s = scope(si);
            auto values_count = s.valuesCount(si);

            if(fn.type == ValueType::Internal_Function){
                if(fn.index == INTERNAL::NOP) //NOP
                {
                }
                else if(fn.index == INTERNAL::LIST) //LIST
                {

                }
                else if(fn.index == INTERNAL::PRINT) //PRINT
                {
                    for(uint32_t i = 1u;i<values_count;i++)
                        cout<< ValueToString( valuesStack[i] ,true) << " ";
                    cout << std::endl;
                }
                else if(fn.index == INTERNAL::plus) //+
                {
                    if(values_count<=1){popScope();return;}
                    #define TYPE_ERROR ERR(cout<<"Type error";)
                    // first value determines which type we are.
                    ValueType initialType = valuesStack[s.values_startIdx+1].type;
                    if(initialType == ValueType::Variable_Number)
                    {
                        double acc = 0;
                        for(uint32_t i = 1u;i<values_count;i++){
                            Value& v = valuesStack[s.values_startIdx+i];
                            if(v.type==ValueType::Variable_String ){
                                if(DBG){ cout <<"STRING:"<< executionStack.readString(v.index)<<" "<<v.index<<endl;cout.flush(); }
                                acc+=stod(executionStack.readString(v.index));
                            }else if(v.type==ValueType::Variable_Number){
                                if(DBG){ cout <<"NUM:"<< executionStack.readNumber(v.index)<<" "<<v.index<<endl;cout.flush(); }
                                acc+=executionStack.readNumber(v.index);
                            }else TYPE_ERROR;
                        }
                        if(DBG){ cout << acc<<endl;cout.flush(); }
                        popScope();
                        pushValue(Value{ValueType::Variable_Number,executionStack.allocNumber(acc)});
                        return;
                    }
                    else if(initialType == ValueType::Variable_String)
                    {
                        string acc = "";
                        for(uint32_t i = 1u;i<values_count;i++){
                            Value& v = valuesStack[s.values_startIdx+i];
                            if(v.type==ValueType::Variable_String ){
                                acc += executionStack.readString(v.index);
                            }else if(v.type==ValueType::Variable_Number){
                                acc += ValueToString(v);
                                // acc+=std::to_string(executionStack.readNumber(v.index));
                            }else TYPE_ERROR;
                        }
                        popScope();
                        pushValue(Value{ValueType::Variable_String,executionStack.allocString(acc)});
                        return;
                    }
                    else TYPE_ERROR;
                    #undef TYPE_ERROR
                }
                else if(fn.index == INTERNAL::minus) //-
                {
                    popScope();
                    return;
                }
                else if(fn.index == INTERNAL::asterisk) //*
                {
                    popScope();
                    return;
                }
                else if(fn.index == INTERNAL::dblEq) //==
                {
                    popScope();
                    return;
                }
                else if(fn.index == INTERNAL::notDblEq) //!=
                {
                    popScope();
                    return;
                }
                else if(fn.index == INTERNAL::LET) //LET
                {
                    if(values_count!=3 && values_count!=4) ERR(cout<<"LET SHOULD HAVE EXACTLY 2 or 3 (typed) ARGS");
                    bool explicitTypeSpecified = (values_count==4);
                    Value ident = valuesStack[s.values_startIdx+1];
                    //cout<<nameToStr(ident.index);
                    if(ident.type != ValueType::Identifier) ERR(cout<<"First arg of LET should be Identifier. Did you forget the ! in front?");
                    Value val = valuesStack[s.values_startIdx+2];

                    optional<Value> explicitType;
                    if(explicitTypeSpecified) explicitType = valuesStack[s.values_startIdx+4];

                    if(val.type==ValueType::Variable_Number){
                        double n = executionStack.readNumber(val.index);
                        popScope();
                        val.index = executionStack.allocNumber(n);
                        pushVariable(val,ident.index);
                    }else if(val.type==ValueType::Variable_String){
                        auto n = executionStack.readString(val.index);
                        popScope();
                        val.index = executionStack.allocString(n);
                        pushVariable(val,ident.index);
                    }else{
                        popScope();
                        pushVariable(val,ident.index);
                    }
                    return;
                }
                else if(fn.index == INTERNAL::eq) //=
                {
                    if(values_count!=3) ERR(cout<<"= SHOULD HAVE EXACTLY 2 ARGS");
                    Value ident = valuesStack[s.values_startIdx+1];
                    //cout<<nameToStr(ident.index);
                    if(ident.type != ValueType::Identifier) ERR(cout<<"First arg of = should be Identifier. Did you forget the ! in front?");

                    long VariableIndex = ResolveIdentifierVariable(ident.index);
                    auto VariableValType = variablesStack[VariableIndex].v.type;
                    auto valIdx = variablesStack[VariableIndex].v.index;
                    Value val = valuesStack[s.values_startIdx+2];

                    if(VariableValType != ValueType::Any_Dynamic && VariableValType != ValueType::Any_Static){
                        if(val.type != VariableValType){
                            ERR(cout<<"Type mismatch.");
                        }
                    }
                    if(val.type==ValueType::Variable_Number){
                        *((double*)executionStack.stackPtr(valIdx)) = executionStack.readNumber(val.index);
                        popScope();
                    }else if(val.type==ValueType::Variable_String){
                        *((string*)executionStack.stackPtr(valIdx)) = executionStack.readString(val.index);
                        popScope();
                    }else{
                        variablesStack[VariableIndex].v = val;
                        popScope();
                        // pushVariable(val,ident.index);
                    }
                    return;
                }else if(fn.index == INTERNAL::DYN){ //DYN
                    if(values_count!=3) ERR(cout<<"LET SHOULD HAVE EXACTLY 2 ARGS");
                    
                    Value ident = valuesStack[s.values_startIdx+1];
                    //cout<<nameToStr(ident.index);
                    if(ident.type != ValueType::Identifier) ERR(cout<<"First arg of LET should be Identifier. Did you forget the ! in front?");
                    Value val = valuesStack[s.values_startIdx+2];
                    if(val.type==ValueType::Variable_Number){
                        double n = executionStack.readNumber(val.index);
                        popScope();
                        val.index = executionStack.allocNumber(n);
                        pushVariable(val,ident.index);
                    }else if(val.type==ValueType::Variable_String){
                        auto n = executionStack.readString(val.index);
                        popScope();
                        val.index = executionStack.allocString(n);
                        pushVariable(val,ident.index);
                    }else{
                        popScope();
                        pushVariable(val,ident.index);
                    }
                    return;
                }else if(fn.index == INTERNAL::FNDECL){ //FNDECL
                    popScope();
                    return;
                }else{
                    ERR(cout << "Unknown internal function: '"<< nameToStr(fnName) <<"' internal fn idx:"<<fn.index<<endl);
                }
            }else if(fn.type == ValueType::Function){ // execute user defined function

            }else{
                ERR(cout << "Type not function or internal function. cant execute"<<endl)
            }
        }
        popScope();
    }

    Compiler(string txt):
        lex(txt),curLexToken(0),executionStack(),
        valuesStack(),variablesStack(),scopesStack()
    {
        Compiler::instance = this;

        __populateExampleTokens(lex);
        lex.parseAll();

        curLexToken = 0;

        scopesStack.push_back(Scope(0,false));
        
        #define put(T,X) pushVariable( Value{ValueType::T, INTERNAL::X}, INTERNAL::X);
        put(Internal_Value_Number,  TRUE);
        put(Internal_Value_Number,  FALSE);
        put(Internal_Function,      NOP);
        put(Internal_Function,      LIST);
        put(Internal_Function,      PRINT);
        put(Internal_Function,      EVAL);
        put(Internal_Function,      ALIAS);
        put(Internal_Function,      eq);
        put(Internal_Function,      plus);
        put(Internal_Function,      minus);
        put(Internal_Function,      asterisk);
        put(Internal_Function,      dblEq);
        put(Internal_Function,      notDblEq);
        put(Internal_Function,      LET);
        put(Internal_Function,      DYN);
        put(Internal_Function,      DYNST);
        put(Internal_Function,      FNDECL);
        #undef put

        bool firstIsFnName = false;
        auto numTokens = lex.parsed.size();
        while (numTokens>curLexToken)
        {
            auto&& s = lex.parsed[curLexToken++];
            if(s.type == s.Symbol){
                if(DBG) cout<<"Symbol:"<<lex.symbols[s.data_index].str<<endl;

                switch (lex.symbols[s.data_index].str[0]){
                case '(':
                    pushScope(true,true);
                    firstIsFnName = true;
                    break;
                case ')':
                    ExecuteFnScopeAndPop();
                    break;
                case '{':
                    pushScope(false,true);
                    break;
                case '}':
                    popScope();
                    break;
                case '[':
                    pushScope(true,true);
                    pushValue({ValueType::Identifier,3}); // LIST
                    break;
                case ']':
                    ExecuteFnScopeAndPop();
                    break;
                }
            }else{
                // string str = lex.stringStorage_entries[];
                if(s.type == s.Identifier){
                    if(DBG) cout<<"Ident:"<<nameToStr(s.data_index)<<endl;   
                    if(firstIsFnName){
                        firstIsFnName = false;
                        pushValue({ValueType::Identifier,s.data_index});
                    }else
                        pushValue(ResolveIdentifierValue(s.data_index)); // push value binded to this name
                }else if(s.type==s.NumberLiteral){
                    if(DBG) cout<<"Num:"<<nameToStr(s.data_index)<<endl;   
                    pushValue(Value{ValueType::Variable_Number,
                        executionStack.allocNumber(nameToStr(s.data_index))
                        });
                }else if(s.type==s.StringLiteral){
                    if(DBG) cout<<"String:"<<nameToStr(s.data_index)<<endl;   
                    pushValue(Value{ValueType::Variable_String,
                        executionStack.allocString( nameToStr( s.data_index ))
                        });
                }else{
                ERR(std::cout << "Error unexpected symbol !!!!!!!!!!!!!!!!!!!");
                break;
                }
            }
        }
        //popScope(); // we are closing anyway..
        
    }


};

