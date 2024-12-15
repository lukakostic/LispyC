#include<string>
#include<vector>
#include<iostream>
#include<algorithm>
#include<unordered_map>

namespace SimpLex {

namespace Util {

    struct StringView
    {
        std::string sourceStr; // source
        int s = -1, e = -1;        // start , end

        StringView(std::string str){
            sourceStr = str;
            s = 0;
            e = str.size();
        }
        StringView(std::string str,int ns, int ne)
            :sourceStr(str),s(ns),e(ne){}

        char operator[](int index){
            return sourceStr[s + index];
        }
        std::string to_string(){
            std::string ss = "";
            for (int c = s; c < e; c++)
                ss.push_back(sourceStr[c]);
            return ss;
        }
        operator std::string(){return to_string();}

        void shiftEnd(int step = 1) { e += step; }
        void shiftStart(int step = 1) { s += step; }
        void shiftBoth(int step = 1){
            s += step;
            e += step;
        }
        void moveBoth(int ns, int ne){
            s = ns;
            e = ne;
        }
        void moveStart(int ns) { s = ns; }
        void moveEnd(int ne) { e = ne; }
        int length() { return std::max(e - s, 0); }
        int size(){ return length(); }
        char lastChar() { return sourceStr[e - 1]; }
        StringView clone(){
            return StringView(sourceStr, s, e);
        }
    };

    template<typename T1, typename T2>
    struct Pair{
        T1 first;
        T2 second;
    };


    struct StringStorage
    {
        #define StringStorage_IdxType uint32_t

        struct Entry{
            StringStorage_IdxType start,end; /*end isnt inclusive*/
            
            std::string to_string(std::string source_allStrings)
                {return StringView(source_allStrings,start,end).to_string(); }
            std::string to_string(StringStorage ss)
                {return to_string(ss.allStrings);}
        };
        
        std::string allStrings;
        
        StringStorage():allStrings(){}

        Entry add(std::string s){
            StringStorage_IdxType start = allStrings.size();
            allStrings.append(s);
            return {start,(StringStorage_IdxType)(start+s.size())};
        }

        #undef StringStorage_IdxType
    };
    struct StringStorageE  //with entries list built-in
    {
        StringStorage ss;
        std::vector<StringStorage::Entry> entries;
        StringStorageE():ss(),entries(){}
        StringStorage::Entry add(std::string s){
            return ss.add(s);
        }
    };


    int isWhitespace(char c){ // 0 if isnt, 1 space, 2 newline
        if(c==' ' || c=='\t') return 1;
        if(c=='\n' || c=='\r' || c=='\0') return 2;
        return 0;
    }
    bool isNumeric(char c){
        return isdigit(c);
    }

};


struct Symbol
{
    uint32_t myIndex; // stationary index the user assigns for his own needs of recognising this symbol

    bool isSplitting = false; // splitting 'X' : "aXb" -> [a,X,b] , non splitting 'X' : "aXb" -> "aXb" . whitespace is always splitting

    std::string str; // string of symbol
};

using Idx_Symbols = uint16_t; // in symbols[]  (registered symbols)
using Idx_ParsedData = uint32_t; //index in some data structure.

struct Parsed
{
    enum Type{
        Symbol, //special (splitting) string
        Identifier, //non-splitting string
        StringLiteral,
        NumberLiteral,
        
        Newline // used if Lex.parseNewlines = true
    } type;
    Idx_ParsedData data_index; //index in some data structure: list of symbols,identifiers,strings,numbers
};



struct Lex;

struct LexMode_Normal
{
    std::string curIdentifier;
    void reset(){
        curIdentifier = "";
    }
    LexMode_Normal(){ reset(); }
    void endCurrentIdentifier(Lex& l); //implemented after struct Lex
    void exitMode(Lex& l){ endCurrentIdentifier(l); }
    Util::Pair<bool,uint32_t> checkSymbols(Lex& l); //implemented after struct Lex
    bool process(Lex& l); //implemented after struct Lex

};
struct LexMode_String
{
    enum Modes{
        Quote,
        DoubleQuote,
        Tick,
        TrippleTick
    } stringMode;

    std::string curString;
    bool inString;

    void reset(){
        curString = "";
        inString = false;
    }
    LexMode_String(){ reset(); }
    void endCurrentIdentifier(Lex& l); //implemented after struct Lex
    void exitMode(Lex& l){ endCurrentIdentifier(l); }
    bool process(Lex& l); //implemented after struct Lex
};
struct LexMode_Number
{
    
    enum Modes{
        Int,
        Real,
        Scientific
    } stringMode;

    std::string curString;

    void reset(){
        curString = "";
    }
    LexMode_Number(){ reset(); }
    void endCurrentNumber(Lex& l); //implemented after struct Lex
    void exitMode(Lex& l){ endCurrentNumber(l); }
    bool process(Lex& l); //implemented after struct Lex
};

struct Lex
{
    static const bool parseNewlines = false; //emit newline tokens to parsed array?
    // use above if you need to know where a newline happens
    // useful for single-line statements like #define in C which ends at newline

    static const bool nestedMultilineComments = true;

    static const bool minusInNumber = true; //only used when minus is right next to number.

    Util::StringView text;

    std::vector<Symbol> symbols; //registered symbols which can be lexxed

    std::vector<Parsed> parsed; // parsed symbols

    Util::StringStorage stringStorage; //identifiers, string literals, number literals
    std::vector<Util::StringStorage::Entry> stringStorage_entries; //identifiers, string literals, number literals
    std::unordered_map<std::string,uint32_t> stringStorage_identifiers; // to track already saved identifiers
    
    bool doDebug = false;

    enum Modes{
        Normal,
        StringLiteral,
        NumberLiteral
    } curMode; //= Normal;
    LexMode_Normal normal_mode;
    LexMode_String string_mode;
    LexMode_Number number_mode;



    Lex(std::string code, bool debug = false):
        text(code+"   \0\0\0\0"),  
        symbols(), parsed(),
        doDebug(debug), curMode(Normal) {}


    void registerNewSymbol(uint32_t myIndex, std::string str,bool splitting, bool INSERT_SORTED = false){
        // if INSERT_SORTED = false, then must call sortRegisteredSymbols after registering all.
        auto&& sym = Symbol{myIndex,splitting,str};
        if(INSERT_SORTED == false || symbols.size()==0)
            symbols.push_back(sym);
        else{
            // binary search for where we should insert our new symbol.
            int strSize = str.size();
            int i, i1 = 0, i2 = symbols.size();
            while(i1!=i2){
                i = (i1+i2)/2; // 3.5 -> 3
                int symSize = symbols[i].str.size();
                if(symSize==strSize){
                    int cmp = symbols[i].str.compare(str);
                    if(cmp==0){// they are the same.. should be an error...
                        i1=i2; // just exit.. good luck user!
                    }else if(cmp>0){ // sym > str
                        if((i1+1)==i2) i1=i2; // to solve infinite loop when i=0, i1=0, i2=1
                        else i1 = i;
                    }else{       // sym < str
                        i2 = i;
                    }
                }else if(symSize>strSize){ // sym.size > str.size
                    //move start (new symbol is too short)
                    if((i1+1)==i2) i1=i2; // to solve infinite loop when i=0, i1=0, i2=1
                    else i1 = i;
                }else if(symSize<strSize){  // sym.size < str.size
                    // move end (new symbol too long)
                    i2 = i;
                }
            }
            symbols.insert( symbols.begin()+i1, sym );
        }
    }

    void pushSymbol(uint16_t symbolIdx){ //add to parsed
        parsed.push_back(Parsed{Parsed::Symbol,symbolIdx});
        if(doDebug) std::cout<<"Symbol:"<<symbols[symbolIdx].str<<std::endl;
    }
    uint32_t regIdent(std::string ident){ //register an identifier
        
        //see if already exists:
        auto found = stringStorage_identifiers.find(ident);
        if(found!=stringStorage_identifiers.end())
            return found->second; //return the stored index
        
        //doesnt already exist:
        uint32_t idx = stringStorage_entries.size();
        stringStorage_entries.push_back(stringStorage.add(ident));
        stringStorage_identifiers[ident] = idx; //save
        return idx;
    }
    void pushIdent(std::string ident){ //add to parsed
        uint32_t idx = regIdent(ident);
        parsed.push_back(Parsed{Parsed::Identifier,idx});
        if(doDebug) std::cout<<"Ident:"<<ident<<std::endl;
    }
    void pushString(std::string str){ //add to parsed
        uint32_t idx = stringStorage_entries.size();
        stringStorage_entries.push_back(stringStorage.add(str));
        parsed.push_back(Parsed{Parsed::StringLiteral,idx});
        if(doDebug) std::cout<<"String:"<<str<<std::endl;
    }    
    void pushNumber(std::string str){ //add to parsed
        uint32_t idx = stringStorage_entries.size();
        stringStorage_entries.push_back(stringStorage.add(str));
        parsed.push_back(Parsed{Parsed::NumberLiteral,idx});
        if(doDebug) std::cout<<"Number:"<<str<<std::endl;
    }   
    // only used if Lex.parseNewlines = true 
    void pushNewline(){    //add to parsed
        //Test for trailing newlines (skip them):
        if(parsed.size()>0 && parsed[parsed.size()-1].type==Parsed::Newline)
            return; //already have 1 newline, we dont need trailing.
        
        parsed.push_back(Parsed{Parsed::Newline,0});
    }

    inline void switchMode(Modes m){
        if(curMode == m) return;

        if(curMode == Normal)
            normal_mode.exitMode(*this);
        else if(curMode == StringLiteral)
            string_mode.exitMode(*this);
        else if(curMode == NumberLiteral)
            number_mode.exitMode(*this);

        curMode = m;
    }
    void parseAll(){
        
        while(text.size()!=0 && text[0]!='\0')
        {
            if(curMode == Normal){
                if(string_mode.process(*this)) continue;
                // normal_mode handles entering number mode.
                //if(number_mode.process(*this)) continue;
                normal_mode.process(*this);
            }else if(curMode == StringLiteral){
                string_mode.process(*this);
            }else if(curMode == NumberLiteral){
                number_mode.process(*this);
            }
        }
    }

    void sortRegisteredSymbols(){ //so longer ones are first, and then lexicographically sort same length ones
        ////Lexically order tokens (1.size, 2.lex. order)
        std::sort(begin(symbols), end(symbols),
            [](const auto &lhs, const auto &rhs){
                return (lhs.str.size() == rhs.str.size()) ? (lhs.str > rhs.str) : (lhs.str.size() > rhs.str.size());
            }
        );
   }
    
};


// Next (like startsWith but with offset): checks if following characters are next in lexer.text
inline bool next(Lex& l, int offset, char c1){
    return (l.text[0+offset] == c1);
}
inline bool next(Lex& l, int offset, char c1, char c2){
    return (l.text[0+offset] == c1)&&(l.text[1+offset] == c2);
}
inline bool next(Lex& l, int offset, char c1, char c2, char c3){
    return (l.text[0+offset] == c1)&&(l.text[1+offset] == c2)&&(l.text[2+offset] == c3);
}
inline bool next(Lex& l, int offset, char c1, char c2, char c3, char c4){
    return (l.text[0+offset] == c1)&&(l.text[1+offset] == c2)&&(l.text[2+offset] == c3)&&(l.text[3+offset] == c4);
}
inline bool next(Lex& l, int offset, std::string s){
    int size = s.size();
    for(int i = 0; i < size; i++){
        if(l.text[i]!=s[i]) return false;
    }
    return true;
}
inline bool next(Lex&l, int offset, char* cstr){
    int i = 0;
    while(*cstr != '\0'){
        if(l.text[i++]!=(*(cstr++))) return false;
    }
    return true;
}


//struct LexMode_Normal {

    void LexMode_Normal::endCurrentIdentifier(Lex& l){
        if(curIdentifier.size()==0) return;
        l.pushIdent(curIdentifier);
        curIdentifier = "";
    }
    
    // symbolDetected? , symbolIndex
    Util::Pair<bool,uint32_t> LexMode_Normal::checkSymbols(Lex& l){
        for(uint16_t i = 0; i<l.symbols.size(); i++){
            if( next(l,0,l.symbols[i].str) )
                return {true,i};
        }
        return {false,0};
    }
    bool LexMode_Normal::process(Lex& l){
        /*
        We are parsing identifiers now, but also checking for comments.
        */
        if(next(l,0,'/','*'))  // entering a comment?  (multi-line)
        {
            endCurrentIdentifier(l);  // in case we were processing an identifier 
            
            if(l.nestedMultilineComments){
                l.text.shiftStart(2); //skip "/*""
                int nestedLevel = 1;
                while(nestedLevel!=0   && !next(l,0,'\0'))
                {
                    if(next(l,0,'*','/')){
                        nestedLevel--;
                        l.text.shiftStart(2);
                    }else if(next(l,0,'/','*')){
                        nestedLevel++;
                        l.text.shiftStart(2);
                    }else l.text.shiftStart(1);
                }            
            }else{
                l.text.shiftStart(2+2); //skip "/*"" and also potential "*/"
                while (!next(l,-2,'*','/') && !next(l,0,'\0')) // we check if previous 2 chars are "*/" so at end of loop we reach just after it
                    l.text.shiftStart(1);
            }

            // our text is now just after the */
            return true;
        }
        else if(next(l,0,'/','/'))   // entering a comment?  (single-line)
        {
            endCurrentIdentifier(l);
            l.text.shiftStart(2+1); //skip "//" and also "\n", we check if previous char is "\n" so at end of loop we reach just after it
            while (!next(l,-1,'\n')    && !next(l,0,'\0'))
                l.text.shiftStart(1);
            // our text is now just after the newline at end of comment.

            if(l.parseNewlines && next(l,-1,'\n')) l.pushNewline();
            return true;
        }else{ // not in a comment, proceed

            int whitespace = Util::isWhitespace(l.text[0]); 
            if(whitespace!=0){
                if(whitespace == 2 && l.parseNewlines) l.pushNewline();
                endCurrentIdentifier(l);
            }else if(curIdentifier.size()==0 && l.text[0]=='.' && Util::isNumeric(l.text[1])){ //should enter number mode!  ".30" -> 0.30
                l.switchMode(l.NumberLiteral);
                l.number_mode.curString = "0.";
                l.text.shiftStart(1);
                return true;
            }else if(Lex::minusInNumber && (curIdentifier.size()==0 && l.text[0]=='-' && 
                (Util::isNumeric(l.text[1]) || (l.text[1]=='.' && Util::isNumeric(l.text[1]))
                ))){ //should enter number mode!  "-3" -> -3
                l.switchMode(l.NumberLiteral);
                l.number_mode.curString = "-";
                l.text.shiftStart(1);
                return true;
            }else if(curIdentifier.size()==0 && Util::isNumeric(l.text[0]) ){ //enter number mode!
                l.switchMode(l.NumberLiteral);
                l.number_mode.curString = "";
                l.text.shiftStart(-1);
            }else{ //
                auto sy = checkSymbols(l); // check if we encountered a symbol
                if(sy.first){ //symbol found?
                    endCurrentIdentifier(l);
                    auto& sym = l.symbols[sy.second]; //get found symbol
                    l.pushSymbol(sy.second);
                    l.text.shiftStart(sym.str.size());
                    return true;
                }else //stil in identifier
                    curIdentifier.push_back(l.text[0]);
            }
            l.text.shiftStart(1);
            return true;
        }
    }

//};
//struct LexMode_String{
    void LexMode_String::endCurrentIdentifier(Lex& l){
        if(inString == false) return;
        //if(curString.size()==0) return;
        l.pushString(curString);
        curString = "";
        inString = false;
    }
    
    bool LexMode_String::process(Lex& l){
        if(l.curMode != Lex::StringLiteral){ //not in string, test entry conditions
            if(next(l,0,'\'')){
                l.switchMode(Lex::StringLiteral);
                stringMode = Quote;
                l.text.shiftStart(1);
                reset();
                inString = true;
                return true;
            }else if(next(l,0,'"')){
                l.switchMode(Lex::StringLiteral);
                stringMode = DoubleQuote;
                l.text.shiftStart(1);
                reset();
                inString = true;
                return true;
            }else if(next(l,0,'`')){
                l.switchMode(Lex::StringLiteral);
                if(next(l,1,'`','`')){
                    stringMode = TrippleTick;
                    l.text.shiftStart(3);
                }else{
                    stringMode = Tick;
                    l.text.shiftStart(1);
                }
                reset();
                inString = true;
                return true;
            }
            return false;
        }else{ //we are in a string already, test exit conditions
            if(stringMode==Quote && next(l,0,'\'')){
                endCurrentIdentifier(l);
                l.text.shiftStart(1);
                l.switchMode(Lex::Normal);
                return true;
            }else if(stringMode==DoubleQuote && next(l,0,'"')){
                endCurrentIdentifier(l);
                l.text.shiftStart(1);
                l.switchMode(Lex::Normal);
                return true;
            }else if(stringMode==Tick && next(l,0,'`')){
                endCurrentIdentifier(l);
                l.text.shiftStart(1);
                l.switchMode(Lex::Normal);
                return true;
            }else if(stringMode==TrippleTick && next(l,0,'`','`','`')){
                endCurrentIdentifier(l);
                l.text.shiftStart(3);
                l.switchMode(Lex::Normal);
                return true;
            }

            //Still in string, add chars:
            if(next(l,0,'\\')){ //escape next
                if     (next(l,1,'\n')){} //escaped newline is ignored.
                if     (next(l,1,' ')){}  //escaped space is ignored.
                else if(next(l,1,'n')) curString.push_back('\n');
                else if(next(l,1,'r')) curString.push_back('\r');
                else if(next(l,1,'t')) curString.push_back('\t');
                else if(next(l,1,'0')) curString.push_back('\0');
                else if(next(l,1,'v')) curString.push_back('\v');
                else if(next(l,1,'a')) curString.push_back('\a');
                else if(next(l,1,'b')) curString.push_back('\b');
                // else if(next(l,1,'e')) curString.push_back('\e');
                else if(next(l,1,'f')) curString.push_back('\f');
                else if(next(l,1,'"')) curString.push_back('"');
                else if(next(l,1,'\'')) curString.push_back('\'');
                else if(next(l,1,'?')) curString.push_back('?');
                else if(next(l,1,'`')) curString.push_back('`');
                else curString.push_back(l.text[1]); //just push whatever literal..
                l.text.shiftStart(1+1); // '\' + escaped characters length (assuming all are 1 char)
            }else{
                // if we encounter a newline, thats implicitly added to text as well.
                curString.push_back(l.text[0]);
                l.text.shiftStart(1);
            }
            return true;
        }
        return false;
    }
//};
//struct LexMode_Number{
  
    void LexMode_Number::endCurrentNumber(Lex& l){
        if(curString.size()==0) return;
        l.pushNumber(curString);
        curString = "";
    }
    bool LexMode_Number::process(Lex& l){
        if(l.curMode != Lex::NumberLiteral){ //test entry conditions
            if(Util::isNumeric(l.text[0])){
                l.switchMode(Lex::NumberLiteral);
                stringMode = Int;
                curString = "";
                curString.push_back(l.text[0]);
                l.text.shiftStart(1);
                return true;
            }
            return false;
        }else{ //we are in a number, test exit conditions
            if(next(l,0,'.')){
                stringMode = Real;
                curString.push_back('.');
                l.text.shiftStart(1);
                return true;
            }else if(next(l,0,'e')){
                stringMode = Scientific;
                curString.push_back('e');
                l.text.shiftStart(1);
                return true;
            }else if(stringMode == Scientific && (next(l,0,'+')||next(l,0,'-'))){ // 1e-10  -> treat the - as part of one number
                curString.push_back(l.text[0]);
                l.text.shiftStart(1);
                return true;
            }else if(Util::isNumeric(l.text[0])==false){
                endCurrentNumber(l);
                // l.text.shiftStart(1);
                l.switchMode(Lex::Normal);
                return true;
            }else{
                curString.push_back(l.text[0]);
                l.text.shiftStart(1);
                return true;
            }
            return false;
        }
        return false;
    }
//};

}

