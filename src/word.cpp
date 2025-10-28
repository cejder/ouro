#include "word.hpp"
#include "math.hpp"
#include "string.hpp"

String *word_generate_name() {
    C8 static const *name_starts[] = {
        "Aen",   "Aeo",  "Aeril", "Aki",  "Al",   "Ane",  "Ar",    "Asu",    "Bal",   "Bao",   "Bar",   "Brev",  "Brik",  "Chen",  "Chi",  "Chon",
        "Cyr",   "Dai",  "Den",   "Drak", "Drev", "Ed",   "El",    "Elor",   "Emi",   "Esha",  "Eth",   "Fael",  "Fen",   "Fumi",  "Gal",  "Gen",
        "Grak",  "Grym", "Hai",   "Han",  "Hek",  "Hika", "Hiro",  "Hisa",   "Ilm",   "Ish",   "Jia",   "Jin",   "Jorn",  "Jun",   "Ka",   "Kael",
        "Kang",  "Kazu", "Ken",   "Kimi", "Kiyo", "Ko",   "Kresh", "Krev",   "Kro",   "Kry",   "Kyo",   "Lao",   "Lia",   "Lin",   "Long", "Lu",
        "Luin",  "Mai",  "Mal",   "Masa", "Mear", "Mei",  "Michi", "Mika",   "Min",   "Ming",  "Miya",  "Mizu",  "Mog",   "Mor",   "Myr",  "Nao",
        "No",    "Nul",  "Nym",   "Oct",  "Oui",  "Park", "Pyr",   "Qel",    "Qian",  "Rael",  "Rel",   "Ren",   "Rhia",  "Rhomb", "Riku", "Ryo",
        "Sachi", "Sada", "Sael",  "Sai",  "Saki", "Ser",  "Sha",   "Shen",   "Shin",  "Sho",   "Shynn", "Sil",   "Skarn", "Skor",  "Sora", "Sor",
        "Sun",   "Suzu", "Ta",    "Taka", "Tama", "Tao",  "Taro",  "Tek",    "Thaea", "Tor",   "Torv",  "Toshi", "Turg",  "Umi",   "Ur",   "Urth",
        "Val",   "Vaz",  "Vea",   "Vik",  "Vor",  "Vulk", "Vyn",   "Wei",    "Wisp",  "Yama",  "Yan",   "Yao",   "Yasu",  "Yo",    "Yori", "Yoshi",
        "Yso",   "Yuki", "Yumi",  "Zan",  "Zarg", "Zel",  "Zen",   "Zephyr", "Zet",   "Zheng", "Zhu",   "Zor",   "Zul",
    };

    C8 static const *name_mids[] = {
        "ad",   "ae",   "ael",  "ag",    "aki",  "ak",   "al",    "an",    "ana",   "ao",    "ar",   "ara",   "asa",   "aul",  "az",    "chi",   "chun",
        "d",    "de",   "dur",  "eb",    "ek",   "el",   "ele",   "eli",   "emn",   "en",    "er",   "es",    "esk",   "est",  "et",    "eth",   "ez",
        "fang", "fumi", "gen",  "go",    "goth", "guo",  "hiko",  "hiro",  "hisa",  "hua",   "hui",  "ia",    "ic",    "id",   "ik",    "il",    "ilo",
        "in",   "ir",   "is",   "isk",   "iso",  "ist",  "ith",   "ji",    "jiang", "jing",  "jun",  "ka",    "kane",  "kang", "katsu", "kazu",  "ki",
        "kimi", "kiyo", "ko",   "lan",   "leng", "lian", "ling",  "long",  "mar",   "maru",  "masa", "michi", "mi",    "min",  "ming",  "mitsu", "na",
        "naga", "naka", "nao",  "nobu",  "nori", "o",    "oe",    "ok",    "ol",    "om",    "omm",  "on",    "oo",    "or",   "ora",   "orb",   "orth",
        "oz",   "peng", "quan", "ri",    "rou",  "sada", "shan",  "sheng", "shi",   "shige", "shin", "sho",   "shu",   "song", "suke",  "t",     "ta",
        "taka", "tama", "tang", "taro",  "tian", "to",   "toshi", "tsu",   "ua",    "ui",    "uk",   "ul",    "um",    "un",   "ur",    "ut",    "uun",
        "van",  "wei",  "wen",  "yan",   "yang", "yao",  "yasu",  "yo",    "ying",  "yn",    "yong", "yori",  "yoshi", "yth",  "yuan",  "yue",   "yuki",
        "yun",  "zaki", "zen",  "zhang", "zhao", "zhen", "zhi",   "zhong", "zhou",  "zhu",   "zong",
    };

    C8 static const *name_ends[] = {
        "a",     "ain",    "ak",    "an",    "ang",    "ani",    "anis", "ant",   "ao",     "ar",    "ari",   "arok",  "as",    "ask",   "azer",  "bashi",
        "bei",   "chun",   "de",    "din",   "duk",    "dur",    "dyl",  "e",     "eff",    "ek",    "el",    "elle",  "emon",  "emor",  "er",    "esh",
        "esis",  "essa",   "eth",   "fang",  "feng",   "fu",     "gahn", "gang",  "gar",    "gawa",  "gong",  "guo",   "guchi", "gur",   "hai",   "han",
        "hao",   "hara",   "hiko",  "hong",  "hua",    "hui",    "i",    "ia",    "id",     "ien",   "ikt",   "is",    "ite",   "ith",   "ium",   "ji",
        "jian",  "jiang",  "jiao",  "jie",   "jin",    "jing",   "jiro", "ju",    "juan",   "jue",   "jun",   "k",     "kage",  "kai",   "kan",   "kang",
        "kao",   "kas",    "ke",    "ken",   "keng",   "ko",     "kong", "kou",   "ku",     "kua",   "kuai",  "kuan",  "kuang", "kui",   "kun",   "kuo",
        "lan",   "lang",   "lao",   "le",    "lea",    "lei",    "leng", "li",    "lia",    "lian",  "liang", "liao",  "lie",   "lin",   "ling",  "liu",
        "long",  "lou",    "lu",    "luan",  "lue",    "lun",    "luo",  "lv",    "lyn",    "man",   "mang",  "mao",   "mar",   "maru",  "mei",   "men",
        "meng",  "mesh",   "mi",    "mian",  "miao",   "mie",    "min",  "ming",  "mir",    "miu",   "mo",    "mori",  "moto",  "mou",   "mu",    "na",
        "nai",   "nak",    "naka",  "nan",   "nang",   "nar",    "nao",  "ne",    "nei",    "nen",   "neng",  "ni",    "nian",  "niang", "niao",  "nie",
        "nin",   "ning",   "niu",   "nong",  "nor",    "nosuke", "nou",  "nu",    "nuan",   "nue",   "nuo",   "nv",    "o",     "odon",  "og",    "ok",
        "on",    "or",     "ora",   "org",   "os",     "ot",     "oth",  "ou",    "pai",    "pan",   "pang",  "pao",   "pei",   "pen",   "peng",  "pi",
        "pian",  "piao",   "pie",   "pin",   "ping",   "po",     "pou",  "pu",    "qi",     "qia",   "qian",  "qiang", "qiao",  "qie",   "qin",   "qing",
        "qiong", "qiu",    "qor",   "qu",    "quan",   "que",    "qun",  "ran",   "rang",   "rao",   "re",    "ren",   "reng",  "ri",    "ro",    "rong",
        "rou",   "ru",     "rua",   "ruan",  "rui",    "run",    "ruo",  "sa",    "saburo", "sai",   "san",   "sang",  "sao",   "sawa",  "se",    "sen",
        "seng",  "sha",    "shai",  "shan",  "shang",  "shao",   "she",  "shei",  "shen",   "sheng", "shi",   "shima", "shou",  "shu",   "shua",  "shuai",
        "shuan", "shuang", "shui",  "shun",  "shuo",   "si",     "song", "sou",   "su",     "suan",  "sui",   "suke",  "sun",   "suo",   "t",     "ta",
        "tai",   "tan",    "tang",  "taro",  "tao",    "te",     "tei",  "teng",  "thas",   "ti",    "tian",  "tiao",  "tie",   "ting",  "to",    "tong",
        "tor",   "tou",    "tu",    "tuan",  "tui",    "tun",    "tuo",  "tusk",  "u",      "uul",   "us",    "uz",    "wa",    "wai",   "wan",   "wang",
        "wei",   "wen",    "weng",  "wo",    "wu",     "wyn",    "ya",   "yama",  "yan",    "yang",  "yao",   "ye",    "yi",    "yin",   "ying",  "yme",
        "yo",    "ylon",   "yong",  "you",   "yth",    "yu",     "yuan", "yue",   "yun",    "z",     "za",    "zai",   "zaki",  "zan",   "zang",  "zao",
        "ze",    "zei",    "zen",   "zeng",  "zha",    "zhai",   "zhan", "zhang", "zhao",   "zhe",   "zhei",  "zhen",  "zheng", "zhi",   "zhong", "zhou",
        "zhu",   "zhua",   "zhuai", "zhuan", "zhuang", "zhui",   "zhun", "zhuo",  "zi",     "zong",  "zou",   "zu",    "zuan",  "zui",   "zun",   "zuo",
    };

    C8 const *start = name_starts[(SZ)random_s32(0, (sizeof(name_starts) / sizeof(name_starts[0])) - 1)];
    C8 const *mid   = random_s32(0, 4) == 0 ? name_mids[(SZ)random_s32(0, (sizeof(name_mids) / sizeof(name_mids[0])) - 1)] : "";
    C8 const *end   = name_ends[(SZ)random_s32(0, (sizeof(name_ends) / sizeof(name_ends[0])) - 1)];

    return TS("%s%s%s", start, mid, end);
}
