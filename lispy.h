/*
    Simple, incomplete LISP implementation.
 */

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>
#include <deque>
#include <iostream>

namespace lispy {

// holds an iterator range to string. Used to avoid string copying when parsing
// and interpretting the input
class str_view {
public:
    typedef std::string::const_iterator const_iterator;

    str_view() : from(), to() {}
    str_view(const std::string &s) : from(s.begin()), to(s.end()) {}
    str_view(const_iterator from, const_iterator to) : from(from), to(to) {}

    const_iterator begin() const { return from; }
    const_iterator end() const { return to; }

    str_view part(const_iterator from, const_iterator to) const {
        return str_view(from, to);
    }

    std::string str() const {
        return std::string(from, to);
    }

    bool operator==(const char *dta) {
        const_iterator i = from;
        for (; *dta && i != to; ++dta, ++i)
        {
            if (*dta != *i)
                return false;
        }

        if (i == to && *dta)
            return false;

        if (i != to && !*dta)
            return false;

        return true;
    }

    const_iterator from, to;
};

struct atom;
struct environment;

/// list
struct list {
    list() {};

    list(const atom &a);

    list(const list &src);

    /// evaluates a list, pushes resulting values into new list
    list(const list &src, environment &env);

    list(list &&src) : car(std::move(src.car)), cdr(std::move(src.cdr)) {
        src.clear();
    }

    void clear() {
        car.reset();
        cdr.reset();
    }

    size_t size() const {
        if (car) {
            if (cdr)
                return 1 + cdr->size();
            else
                return 1;
        } else {
            return 0;
        }
    }

    bool empty() const {
        return !car && !cdr;
    }

    static const list Empty;

    const atom &operator[](size_t idx) const;

    atom &operator[](size_t idx);

    const atom &front() const;
    atom &front();

    const list &rest() const {
        if (cdr)
            return *cdr;

        return list::Empty;
    }

    list eval_rest();

    struct const_iterator {
        const_iterator(const list * l = nullptr) : node(l) {}

        const atom &operator*() {
            assert(node);
            return node->front();
        }

        const atom *operator->() {
            assert(node);
            return &node->front();
        }

        const_iterator operator++() {
            if (end())
                return *this;
            node = node->cdr.get();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator last = *this;
            operator++();
            return last;
        }

        bool operator!=(const const_iterator &other) const {
            return node != other.node;
        }

        bool operator==(const const_iterator &other) const {
            return node == other.node;
        }



        bool end() {
            return node == NULL;
        }

        const list *node;
    };

    const_iterator begin() const {
        return const_iterator(this);
    }

    const_iterator end() const {
        return const_iterator();
    }

    void pop_front();

    void push_back(const atom &a);
    void push_back(atom &&a);

    std::unique_ptr<atom> car;
    std::unique_ptr<list> cdr;
};

const list list::Empty;

bool toNumber(str_view val, int &i) {
    try {
        i = std::stoi(val.str(), 0, 0);
        return true;
    } catch (const std::invalid_argument &) {
        return false;
    }
}

struct environment;

/** atomic value - simple variant type implementation */
class atom {
public:
    // value type
    enum atom_type {
        NIL = 0,
        INT = 1,
        STR = 3,
        LST = 4,
        PRC = 5
    };

    static const char* strtype(atom_type t) {
        switch (t) {
        case NIL: return "NIL";
        case INT: return "INT";
        case STR: return "STR";
        case LST: return "LST";
        case PRC: return "PRC";
        }
    }

    atom_type t;
    typedef std::string string;
    typedef std::function<atom (environment &env, const atom &)> proc;

    union {
        int iv;
        string sv;
        list lv;
        proc fv;
    };

    atom() : t(NIL) {
    }

    atom(atom_type t) : t(t) {
        switch (t) {
        case NIL:
            return;
        case INT:
            new (&iv) int();
            return;
        case STR:
            new (&sv) string();
            return;
        case LST:
            new (&lv) list();
            return;
        case PRC:
            new (&fv) proc();
            return;
        }
    }

    atom(atom &&src) : t(src.t) {
        switch (t) {
        case NIL:
            return;
        case INT:
            new (&iv) int(std::move(src.iv));
            return;
        case STR:
            new (&sv) string(std::move(src.sv));
            return;
        case LST:
            new (&lv) list(std::move(src.lv));
            return;
        case PRC:
            new (&fv) proc(std::move(src.fv));
            return;
        }
        src.clear();
    }

    atom(const atom &src) : t(src.t) {
        switch (t) {
        case NIL:
            return;
        case INT:
            new (&iv) int(src.iv);
            return;
        case STR:
            new (&sv) string(src.sv);
            return;
        case LST:
            new (&lv) list(src.lv);
            return;
        case PRC:
            new (&fv) proc(src.fv);
            return;
        }
    }

    atom(int i) {
        t = INT;
        iv = i;
    }

    atom(const string &s) {
        t = STR;
        new (&sv) string(s);
    }

    atom(const char *c) {
        t = STR;
        new (&sv) string(c);
    }

    atom(const proc &p) {
        t = PRC;
        new (&fv) proc(p);
    }

    atom(const list &l) {
        t = LST;
        new (&lv) list(l);
    }

    atom(const list &l, environment &env) {
        t = LST;
        new (&lv) list(l, env);
    }

    atom(list &&l) {
        t = LST;
        new (&lv) list(l);
    }

    ~atom() {
        clear();
    }

    atom &operator=(const proc &p) {
        clear();
        t = PRC;
        new (&fv) proc(p);
    }

    static const atom True;
    static const atom False;
    static const atom Nil;

    void clear() {
        switch (t) {
        case NIL:
            return;
        case INT:
            return;
        case STR:
            sv.~string();
            return;
        case LST:
            lv.~list();
            return;
        case PRC:
            fv.~proc();
            return;
        }
        t = NIL;
    }

    atom &operator=(const atom &src) {
        clear();
        t = src.t;

        switch (t) {
        case NIL:
            return *this;
        case INT:
            new (&iv) int(src.iv);
            return *this;
        case STR:
            new (&sv) string(src.sv);
            return *this;
        case LST:
            new (&lv) list(src.lv);
            return *this;
        case PRC:
            new (&fv) proc(src.fv);
            return *this;
        }
    }

    //// Will not construct a list!
    atom(const str_view &token) {
        // first char == '(' - a list. Descend while tokenizing
        // is it a number?
        if (toNumber(token, iv)) {
            t = INT;
        } else {
            new (&sv) std::string();
            sv = token.str();
            t = STR;
        }
    }

    void expect(atom_type typ) const {
        if (t != typ)
            throw std::invalid_argument(std::string("Unexpected type ")
                                        + strtype(typ) + ", mine "
                                        + strtype(t));
    }

    int asInt() const {
        expect(INT);
        return iv;
    }

    const string &asString() const {
        expect(STR);
        return sv;
    }

    const list &asList() const {
        expect(LST);
        return lv;
    }

    const atom &operator[](int idx) const {
        expect(LST);
        if (idx >= size())
            return Nil;
        return lv[idx];
    }

    int &asInt() {
        expect(INT);
        return iv;
    }

    string &asString() {
        expect(STR);
        return sv;
    }

    list &asList() {
        expect(LST);
        return lv;
    }

    atom &operator[](int idx) {
        expect(LST);
        return lv[idx];
    }

    size_t size() const {
        return lv.size();
    }

    void push_back(const atom &a) {
        asList().push_back(a);
    }

    std::string repr(const std::string &indent = "") const {
        switch (t) {
        case NIL:
            return "nil";
        case INT:
            return std::to_string(iv);
        case STR:
            return '"' + sv + '"';
        case LST: {
            std::string result = "(";
            bool frst = true;
            for (const atom& a : lv) {
                if (!frst) result += ' ';
                frst = false;
                result += a.repr();
            }
            result += ")";
            return result;
        }
        case PRC:
            return "PROC";
        }
    }

    atom eval(environment &env) const;

    atom operator()(environment &env, const atom &values) {
        expect(PRC);
        return fv(env, values);
    }

    atom evalRest(environment &env) const {
        return atom(lv.rest(), env);
    }

    atom front() const {
        return atom(lv.front());
    }

    atom rest() const {
        return atom(lv.rest());
    }

    bool operator==(const atom &b) {
        if (t != b.t)
            return false;
        switch (t) {
        case NIL:
            return true;
        case INT:
            return iv == b.iv;
        case STR:
            return sv == b.sv;
        case LST:
            // TODO!
            return false;
        case PRC:
            // TODO!
            return false;
        }
    }
};

const atom atom::True("#t");
const atom atom::False("#f");
const atom atom::Nil;

list::list(const atom &a) {
    push_back(a);
}

list::list(const list &src) : car(src.car ? new atom(*src.car) : nullptr),
                              cdr(src.cdr ? new list(*src.cdr) : nullptr)
{}

list::list(const list &src, environment &env)
    : car(src.car ? new atom(src.car->eval(env)) : nullptr),
      cdr(src.cdr ? new list(*src.cdr) : nullptr)
{}

const atom &list::front() const {
    if (car)
        return *car;
    return atom::Nil;
}

atom &list::front() {
    return *car;
}

const atom &list::operator[](size_t idx) const {
    if (idx == 0)
        return front();
    else {
        if (cdr)
            return (*cdr)[idx - 1];
        else
            return atom::Nil;
    }
}

atom &list::operator[](size_t idx) {
    if (idx == 0)
        return front();
    else {
        if (cdr)
            return (*cdr)[idx - 1];
        else
            throw std::out_of_range("List index out of range");
    }
}

void list::pop_front() {
    if (cdr) {
        car = std::move(cdr->car);
        cdr = std::move(cdr->cdr);
    } else {
        car.reset(new atom(atom::Nil));
        cdr.reset();
    }
}

void list::push_back(const atom &a) {
    if (car) {
        if (cdr)
            cdr->push_back(a);
        else
            cdr.reset(new list(a));
    } else {
        if (cdr)
            throw std::runtime_error("CAR not null, CDR set!");

        car.reset(new atom(a));
    }
}

void list::push_back(atom &&a) {
    if (car) {
        if (cdr) {
            cdr->push_back(a);
        } else {
            cdr.reset(new list(std::move(a)));
            a.clear();
        }
    } else {
        if (cdr)
            throw std::runtime_error("CAR not null, CDR set!");

        car.reset(new atom(std::move(a)));
        a.clear();
    }
}

/** tokenizes input. Converts parts of string to literals, values,
    open/close operations */
class tokenizer {
public:
    tokenizer(const str_view &sv)
        : sv(sv), si(sv.begin())
    {}

    bool has_next() {
        return si != sv.end();
    }

    str_view next() {
        if (si == sv.end())
            return str_view();

        // eat any spaces
        while (si != sv.end() && ::isspace(*si))
            ++si;

        str_view::const_iterator cur = si;

        // token will be the contents of the braces
        if (*si == '(' || *si == ')') {
            // emit the brace
            ++si;
            return str_view(cur, si);
        }

        // token - anything till next whitespace
        while (si != sv.end() && !::isspace(*si) && *si != '(' && *si != ')')
            ++si;

        return str_view(cur, si);
    }

    str_view peek_next() {
        tokenizer cpy(*this);
        return cpy.next();
    }

    str_view sv;
    str_view::const_iterator si;
};

struct environment {
    typedef std::map<std::string, atom> map;

    environment(std::shared_ptr<environment> parent)
        : outer(outer)
    {}

    environment()
        : outer()
    {}

    atom eval(const atom &src) {
        return src.eval(*this);
    }

    atom &operator[](const std::string &key) {
        map::iterator i = values.find(key);
        if (i != values.end())
            return i->second;

        if (outer)
            return (*outer)[key];

        // create a new atom entry
        return values[key];
    }

    const atom &operator[](const std::string &key) const {
        map::const_iterator i = values.find(key);
        if (i != values.end())
            return i->second;

        if (outer)
            return (*outer)[key];

        return atom::Nil;
    }

    map values;
    std::shared_ptr<environment> outer;
};

atom atom::eval(environment &env) const {
    switch (t) {
    case NIL:
        return *this;
    case INT:
        return *this;
    case STR:
        return env[sv];
    case LST:
        if (lv.empty())
            return Nil;
        // evaluate by finding proc for first element
        return env[lv.begin()->asString()](env, rest());
    case PRC:
        return fv(env, evalRest(env));
    }
}

void bind_std(environment &env) {
    env["nil"] = atom::Nil;
    env["#t"] = atom::True;
    env["#f"] = atom::False;
    env["if"] = [](environment &env, const atom &params) {
        return params[0].eval(env) == atom::False ?
        (params[2].eval(env)) :
        (params[1].eval(env));
    };

    // TODO: These should respect the environment of the atom in question
    env["set!"] = [](environment &env, const atom &params) {
        return env[params[0].asString()] = params[1].eval(env);
    };

    env["define"] = [](environment &env, const atom &params) {
        return env[params[0].asString()] = params[1].eval(env);
    };

    env["env"] = [](environment &env, const atom &) {
        atom aenv(atom::LST);
        for (const auto &kv : env.values) {
            atom val(atom::LST);
            val.push_back(atom(kv.first));
            val.push_back(kv.second);
            aenv.push_back(val);
        }
        return aenv;
    };

    env["quote"] = [](environment &env, const atom &v) {
        return v[0];
    };

    env["eval"] = [](environment &env, const atom &v) {
        return env.eval(v[0]);
    };

    env["car"] = [](environment &env, const atom &v) {
        return v[0].eval(env).front();
    };

    env["cdr"] = [](environment &env, const atom &v) {
        return v[0].eval(env).rest();
    };

    env["*"] = [](environment &env, const atom &v) {
        int res = 1;
        for (const atom &a : v.asList()) {
            res *= a.eval(env).asInt();
        }
        return atom(res);
    };

    env["+"] = [](environment &env, const atom &v) {
        int res = 0;
        for (const atom &a : v.asList()) {
            res += a.eval(env).asInt();
        }
        return atom(res);
    };

    env["-"] = [](environment &env, const atom &v) {
        const list &lst = v.asList();
        int res = lst.front().eval(env).asInt();
        for (const atom &a : lst.rest()) {
            res -= a.eval(env).asInt();
        }
        return atom(res);
    };

    env["/"] = [](environment &env, const atom &v) {
        const list &lst = v.asList();
        int res = lst.front().eval(env).asInt();
        for (const atom &a : lst.rest()) {
            res /= a.eval(env).asInt();
        }
        return atom(res);
    };

    env["<"] = [](environment &env, const atom &v) {
        const list &lst = v.asList();
        int res = lst.front().eval(env).asInt();
        for (const atom &a : lst.rest()) {
            if (res > a.eval(env).asInt())
                return atom::False;
        }
        return atom::True;
    };

    env[">"] = [](environment &env, const atom &v) {
        const list &lst = v.asList();
        int res = lst.front().eval(env).asInt();
        for (const atom &a : lst.rest()) {
            if (res < a.eval(env).asInt())
                return atom::False;
        }
        return atom::True;
    };

}

atom build_from(tokenizer &t) {
    // nill
    if (!t.has_next())
        return atom();

    // peek first token
    str_view tok = t.next();

    if (tok == "(") {
        atom a(atom::LST);

        while (t.has_next()) {
            tok = t.peek_next();

            if (tok == ")") {
                t.next();
                break;
            }

            atom inner = build_from(t);
            a.asList().push_back(inner);
        }

        return a;
    } else {
        return atom(tok);
    }
}

void exec(environment &env, const std::string &expr) {
    str_view sv(expr);

    tokenizer t(sv);

    while (t.has_next()) {
        atom parsed = build_from(t);
        // evaluate via environment
        std::cout << env.eval(parsed).repr() << std::endl;
    }
}

} // namespace lispy
