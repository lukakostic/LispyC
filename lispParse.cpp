/*
"C is high level assembly"  nije istina, 
posebno zato sto assembly se mnogo brzo i lako parsira dok C ne toliko.
zato bi bilo lepo da napravim neki kod koj je ekvivalentan C-u a parsira se mnogo brze i lakse.
#######################################################
Ideja je da bude neki program koj se neverovatno brzo leksira + parsira.

Nema nested strukture odma nego sve tutkam na stek, 
pa onda kasnije iskopiram sve sa steka u tree strukturu.
#######################################################
najbrzi ove vrste bi bio kao assembler, 
gde je vec poznato za svaku operaciju koliko se simbola ocekuje unapred
ili bi ja morao da dam tipa broj unapred indicirajuci broj argumenata
a operacije/ids nisu pisane slovima (proizvoljne duzine) vec bi bili indeksi tj uint16

a lisp je nicely ovakav (doduse sa recima a ne uint32) jer sve odvojeno,
ali jos bolje bi bio stack machine.

###################################################### lisp
(class KL {     // 1 group inside (1 method)
(func  FN (a b (= c 3)) {
  (let z 3)                               ; let z = 3
  (= z 5)                                 ; z = 5
  { z .y "test" .id 3 }     ;  z.y["test"].id[3] = 5
  (= (: z.y "test" .id 3) 5)     ;  z.y["test"].id[3] = 5
  
  (return (+ z (- b c)))             ; z + (b-c)
})
})

(for (let x 0) (< x size) (++ x) {
     (if x break)                           ; if(x){ break; }
})



{ } =  not expecting function/operator in first position.
[ ]  =  list
( ) = expecting function/operator in first position.

{} se koristi na kraju for, if, itd.
ali ako se koristi unutar body, onda predstavlja pristup parametru.

(list  a b c)    = list(a,b,c)
(fn  (: a 3)

#rules of language:  
sve zagrade su matched. ne postoje < > zagrade.
if its one word it doesnt need braces.
if its multiple words it NEEDS braces.
a.x    mora  (a.x)
a = 3   mora  (= a 3)

struct, func, fn-name, naziv tipova  SVI MORAJU PRVI u zagradi.
znaci ono prvo u zagradi je sta vrsi operaciju.


###################################################### lisp-stack-machine
(KL {     // 1 group inside (1 method)
(func  FN (a b (c 3 =)) {
  (z 3 let)                               ; let z = 3
  (z 5 =)                                 ; z = 5
  ((((z 'y' .) 'test' .) 'id' .) 3 .)
  ;{ z .y "test" .id 3 }     ;  z.y["test"].id[3] = 5
  ;(= (: z.y "test" .id 3) 5)     ;  z.y["test"].id[3] = 5
  
  ((z (b c -) +) return)             ; z + (b-c)
})
} class)

((x 0 let) (x size <) (x ++) {
     ({break} x if)                           ; if(x){ break; }
} for)


Mada nema poente u stack machine. Uzasno je za pisanje,
a ja ionako ne izvrsavam direktno metode (il dajem asembleru)
vez prolazi kroz kompajler koj radi pametne stvari, pa tek onda on izbaci sta treba.
a kompajler vec zna koja je operacija i koliko parametra uzima,  i sam generise stek masinu.

A ni za assembly nije dobro jer nemamo stvarno stek vec imamo registre.
A registre ne push/pop tako da meh.

jedino bi bilo dobro ako bi imali VM stek masinu za koju generisemo instrukcije.
*/
#include "lispParse.hpp"

/*
struct Node  // Symbol or Tree
{
    struct Symbol_t
    {
        uint32_t idx;  // index to what? depends on type. Eg Identifier vs Terminal vs StringLiteral vs NumberLiteral
    };
    struct Tree_t
    {
        uint32_t treeStartIdx;
        uint16_t count;
    };

    union {
        Symbol_t sym;
        Tree_t tree;
    } data;
    enum : unsigned char {
        //Symbol (sym):
        // Symbol_Terminal, // operator
        Symbol_Identifier,
        Symbol_StringLiteral,
        Symbol_NumberLiteral,
        Symbol_Newline,
        //Tree (tree)
        Tree_Parentheses,
        Tree_Bracket,
        Tree_Curly,
        Tree_Idk,
    } type;
};
*/


Compiler* Compiler::instance = nullptr;

int main(int argc,char** argv){
    string txt = readFileIntoString("lisp_test.txt"); //string(argv[1])); //read first arg. 0th is prog name
    
    Compiler exe(txt);
    //lex.DBG = true;

    /*
    int reservedWordsIdx = -1;
    {
        auto&& l = lex;
        //___pushIdents("new", "free", "delete", "for", "while", "return", "break", "continue", "else", "elif", "try", "finally", "switch", "case");
        ___pushIdents("new", "delete", "class", "for", "while", "return", "break", "continue", "if", "else", //"elif",
         "try", "catch","finally", "switch", "case", "import","from","as",//"with","using", "struct"
         "const","let","var","async","await","function","export","default",
         //"constructor", "this", // ?
         "null","undefined","true","false"
         );
        reservedWordsIdx = l.stringStorage_entries.size();
    }
    */

//    auto&& lex = exe.lex;
    
    /*
    stringstream ss;   
    ss<<"{\"symbols\":[";
    for(int i = 0; i<lex.symbols.size();i++){
        ss<<"\"";
        escapedJSON(lex.symbols[i].str,ss);
        ss<<"\"";
        if(i!=(lex.symbols.size()-1)) ss<<",";
    }
    ss<<"],\"parsed\":";

    ss<<"[";

    for(int j = 0; j <lex.parsed.size(); j++){
        auto& s = lex.parsed[j];
        if(s.type == s.Symbol){
            ss << "[0,\"";
            escapedJSON(lex.symbols[s.data_index].str, ss);
            ss << "\"]";
            //std::cout<<"Symbol:"<< lex.symbols[s.data_index].str << std::endl;
        }else if(s.type == s.Identifier){
            auto sx = lex.stringStorage_entries[s.data_index].to_string(lex.stringStorage);
            //std::cout<<"Ident:"<< Util::StringView(lex.stringStorage.allStrings,e.start,e.end).to_string() << std::endl;
            if(s.data_index<reservedWordsIdx) ss<<"[0,\"";
            else ss << "[1,\"";
            escapedJSON(sx,ss);
            ss << "\"]";
        }else if(s.type == s.StringLiteral){
            //std::cout<<"String:"<< Util::StringView(lex.stringStorage.allStrings,e.start,e.end).to_string() << std::endl;
            auto sx = lex.stringStorage_entries[s.data_index].to_string(lex.stringStorage);
            ss << "[2,\"";
            escapedJSON(sx,ss);
            ss << "\"]";
        }else if(s.type == s.NumberLiteral){
            auto sx = lex.stringStorage_entries[s.data_index].to_string(lex.stringStorage);
            //std::cout<<"Number:"<< Util::StringView(lex.stringStorage.allStrings,e.start,e.end).to_string() << std::endl;
            ss << "[3,";
            ss << sx;
            ss << "]";
        }
        else if(s.type == s.Newline){
            ss << "[4,0]";
        }

        if(j != (lex.parsed.size()-1)) 
            ss << ",";
            //ss << ",\n";
    }
    ss << "]";

    ss<<"}";
    std::cout << ss.str();
    */
    
    return 0;
}