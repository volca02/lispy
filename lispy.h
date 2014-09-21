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
std::shared_ptr<environment> clone_environment(
        const std::shared_ptr<environment> &env);

/// list
struct list {
    list() : car(), cdr() {};

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

    list &operator=(list &&other);

    const atom &front() const;
    atom &front();

    const list &rest() const {
        if (cdr)
            return *cdr;

        return list::Empty;
    }

    list eval_rest();

    struct const_iterator {
        const_iterator(const list * l = nullptr) : node(l) {
            // step to next if the current has nil car empty, in fact
            if (node && !node->car)
                node = nullptr;
        }

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
            return !node;
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
    void append(atom &&a);

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
        STR = 2,
        LST = 3,
        PRC = 4,
        LMB = 5
    };

    static const char* strtype(atom_type t) {
        switch (t) {
        case NIL: return "NIL";
        case INT: return "INT";
        case STR: return "STR";
        case LST: return "LST";
        case PRC: return "PRC";
        case LMB: return "LMB";
        }
        return "<INVALID>";
    }

    typedef std::string string;
    typedef std::function<atom (environment &env, const atom &)> proc;

    atom_type type() const {
        return t;
    }

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
        case LMB:
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
            break;
        case INT:
            new (&iv) int(std::move(src.iv));
            break;
        case STR:
            new (&sv) string(std::move(src.sv));
            break;
        case LMB:
            env = std::move(src.env);
        case LST:
            new (&lv) list(std::move(src.lv));
            break;
        case PRC:
            new (&fv) proc(std::move(src.fv));
            break;
        }
        // don't call clear here!
        src.t = NIL;
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
        case LMB:
            env = clone_environment(src.env);
        case LST:
            new (&lv) list(src.lv);
            return;
        case PRC:
            new (&fv) proc(src.fv);
            return;
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
        new (&lv) list(std::move(l));
    }

    ~atom() {
        clear();
    }

    atom &operator=(const proc &p) {
        clear();
        t = PRC;
        new (&fv) proc(p);
        return *this;
    }

    atom &operator=(atom &&a) {
        clear();

        t = a.t;
        switch (a.t) {
        case NIL:
            break;
        case INT:
            iv = a.iv;
            break;
        case STR:
            new (&sv) std::string(std::move(a.sv));
            break;
        case LMB:
            env = std::move(a.env);
        case LST:
            new (&lv) list(std::move(a.lv));
            break;
        case PRC:
            new (&fv) proc(std::move(a.fv));
            break;
            break;
        }

        a.t = NIL;
        return *this;
    }

    static const atom True;
    static const atom False;
    static const atom Nil;

    void clear() {
        switch (t) {
        case NIL:
            break;
        case INT:
            break;
        case STR:
            sv.~string();
            break;
        case LMB:
            env.reset();
        case LST:
            lv.~list();
            break;
        case PRC:
            fv.~proc();
            break;
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
        case LMB:
            env = clone_environment(src.env);
        case LST:
            new (&lv) list(src.lv);
            return *this;
        case PRC:
            new (&fv) proc(src.fv);
            return *this;
        }
        return *this;
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

    const atom &operator[](size_t idx) const {
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
        std::string result;
        switch (t) {
        case NIL:
            return "nil";
        case INT:
            return std::to_string(iv);
        case STR:
            return '"' + sv + '"';
        case LMB:
            result = "<Lambda>";
        case LST: {
            result += "(";
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
        return "<INVALID>";
    }

    atom eval(environment &env) const;

    atom operator()(environment &env, const atom &values);

    atom evalRest(environment &env) const {
        return atom(lv.rest(), env);
    }

    atom evalEach(environment &env) const {
        return atom(lv, env);
    }

    atom front() const {
        expect(LST);
        return atom(lv.front());
    }

    atom rest() const {
        expect(LST);
        return atom(lv.rest());
    }

    atom length() const {
        expect(LST);
        return atom(lv.size());
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
        case LMB:
        case LST:
            // TODO!
            return false;
        case PRC:
            // TODO!
            return false;
        }
        return false;
    }

    const atom &lambda_args() const {
        expect(LMB);
        return lv[0];
    }

    const atom &lambda_body() const {
        expect(LMB);
        return lv[1];
    }

    atom toLambda(const environment &env) const;

private:
    atom_type t;
    std::shared_ptr<environment> env;
    union {
        int iv;
        string sv;
        list lv;
        proc fv;
    };
};

const atom atom::True("#t");
const atom atom::False;
const atom atom::Nil;

list::list(const list &src) : car(src.car ? new atom(*src.car) : nullptr),
                              cdr(src.cdr ? new list(*src.cdr) : nullptr)
{}

list::list(const list &src, environment &env)
    : car(src.car ? new atom(src.car->eval(env)) : nullptr),
      cdr(src.cdr ? new list(*src.cdr, env) : nullptr)
{}

list &list::operator=(list &&a) {
    std::swap(car, a.car);
    std::swap(cdr, a.cdr);
    a.clear();
    return *this;
}

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
        else {
            cdr.reset(new list());
            cdr->push_back(a);
        }
    } else {
        if (cdr)
            throw std::runtime_error("CAR not null, CDR set!");

        car.reset(new atom(a));
    }
}

void list::append(atom &&a) {
    // TODO: append internal, so we won't check this every step
    if (a.type() != atom::LST) {
        push_back(a);
        return;
    }

    if (car) {
        if (cdr)
            cdr->append(std::move(a));
        else {
            cdr.reset(new list());
            *cdr = std::move(a.asList());
            a.clear();
        }
    } else {
        if (cdr)
            throw std::runtime_error("CAR not null, CDR set!");

        list mv = std::move(a.asList());
        car = std::move(mv.car);
        cdr = std::move(mv.cdr);
        a.clear();
    }
}

void list::push_back(atom &&a) {
    if (car) {
        if (cdr) {
            cdr->push_back(a);
        } else {
            cdr.reset(new list());
            cdr->push_back(std::move(a));
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
        : outer(parent)
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

        throw std::invalid_argument("No symbol with name " + key);
    }

    const atom &operator[](const std::string &key) const {
        map::const_iterator i = values.find(key);
        if (i != values.end())
            return i->second;

        if (outer)
            return (*outer)[key];

        throw std::invalid_argument("No symbol with name " + key);
    }

    atom &set(const std::string &key) {
        map::iterator i = values.find(key);
        if (i != values.end())
            return i->second;

        if (outer) {
            i = outer->values.find(key);
            if (i != outer->values.end())
                return i->second;
        }

        // create new entry
        return values[key];
    }

    map values;
    std::shared_ptr<environment> outer;
};

std::shared_ptr<environment> clone_environment(
        const std::shared_ptr<environment> &env)
{
    if (env)
        return std::make_shared<environment>(*env);
    else
        return std::shared_ptr<environment>();
}

atom atom::eval(environment &env) const {
    switch (t) {
    case NIL:
        return *this;
    case INT:
        return *this;
    case STR:
        return env[sv];
    case LMB:
    {
        return lambda_body().eval(env);
    }
    case LST: {
        // empty list evaluates to itself
        if (lv.empty())
            return *this;

        atom result = env[lv.begin()->asString()](env, rest());
#ifdef LISPY_DEBUG
        // evaluate by finding proc for first element
        std::cout << "Eval  "   << repr()
                  << " with "   << lv.begin()->asString()
                  << " args "   << rest().repr()
                  << " via "    << env[lv.begin()->asString()].repr()
                  << " yields " << result.repr()
                  << std::endl;
#endif
        return result;
    }
    case PRC:
        return fv(env, evalRest(env));
    }
    return Nil;
}

atom atom::operator()(environment &current_env, const atom &values) {
    if (t == PRC)
        return fv(current_env, values);
    else if (t == LMB) {
        if (!env)
            throw std::invalid_argument(
                    "Lambda is missing environment");

        // iterate lambda_args, fill all with values
        list::const_iterator vit = values.asList().begin();
        for (const auto &larg : lambda_args().asList()) {
            if (vit == values.asList().end()) {
                throw std::invalid_argument(
                        "Lambda call with incomplete arguments");
            }

            env->set(larg.asString()) = *vit++;
        }

        return lambda_body().eval(*env);
    }

    throw std::invalid_argument(
            "Could not eval");
}

atom atom::toLambda(const environment &env) const {
    expect(LST);
    if (lv.size() != 2)
        throw std::invalid_argument(
                "Lambda definition needs two list params");

    if (lv[0].t != LST)
        throw std::invalid_argument(
                "Lambda definition's first arg has to be list of args");

    if (lv[1].t != LST)
        throw std::invalid_argument(
                "Lambda definition's second arg has to be the body");

    // special conversion here
    atom cpy(*this);
    cpy.t = LMB;
    cpy.env.reset(new environment(env));
    return cpy;
}


void bind_std(environment &env) {
    env.set("nil") = atom::Nil;
    env.set("#t") = atom::True;
    env.set("#f") = atom::False;
    env.set("if") = [](environment &env, const atom &params) {
        return params[0].eval(env) == atom::False ?
        (params[2].eval(env)) :
        (params[1].eval(env));
    };

    // TODO: These should respect the environment of the atom in question
    env.set("set!") = [](environment &env, const atom &params) {
        return env.set(params[0].asString()) = params[1].eval(env);
    };

    env.set("setq") = [](environment &env, const atom &params) {
        return env.set(params[0].asString()) = params[1];
    };

    env.set("lambda") = [](environment &env, const atom &params) {
        return params.toLambda(env);
    };

    env.set("define") = [](environment &env, const atom &params) {
        return env.set(params[0].asString()) = params[1].eval(env);
    };

    env.set("env") = [](environment &env, const atom &) {
        atom aenv(atom::LST);
        for (const auto &kv : env.values) {
            atom val(atom::LST);
            val.push_back(atom(kv.first));
            val.push_back(kv.second);
            aenv.push_back(val);
        }
        return aenv;
    };

    env.set("quote") = [](environment &env, const atom &v) {
        return v[0];
    };

    env.set("list") = [](environment &env, const atom &v) {
        return v.evalEach(env);
    };

    env.set("length") = [](environment &env, const atom &v) {
        return v[0].eval(env).length();
    };

    env.set("eval") = [](environment &env, const atom &v) {
        return v[0].eval(env).eval(env);
    };

    env.set("append") = [](environment &env, const atom &v) {
        atom ev = v[0].eval(env);
        atom lst = ev[0];
        lst.asList().append(lst.rest());
        return lst;
    };

    env.set("car") = [](environment &env, const atom &v) {
        return v[0].eval(env).front();
    };

    env.set("cdr") = [](environment &env, const atom &v) {
        return v[0].eval(env).rest();
    };

    env.set("*") = [](environment &env, const atom &v) {
        int res = 1;
        for (const atom &a : v.asList()) {
            res *= a.eval(env).asInt();
        }
        return atom(res);
    };

    env.set("+") = [](environment &env, const atom &v) {
        int res = 0;
        for (const atom &a : v.asList()) {
            res += a.eval(env).asInt();
        }
        return atom(res);
    };

    env.set("-") = [](environment &env, const atom &v) {
        const list &lst = v.asList();
        int res = lst.front().eval(env).asInt();
        for (const atom &a : lst.rest()) {
            res -= a.eval(env).asInt();
        }
        return atom(res);
    };

    env.set("/") = [](environment &env, const atom &v) {
        const list &lst = v.asList();
        int res = lst.front().eval(env).asInt();
        for (const atom &a : lst.rest()) {
            res /= a.eval(env).asInt();
        }
        return atom(res);
    };

    env.set("<") = [](environment &env, const atom &v) {
        const list &lst = v.asList();
        int res = lst.front().eval(env).asInt();
        for (const atom &a : lst.rest()) {
            if (res > a.eval(env).asInt())
                return atom::False;
        }
        return atom::True;
    };

    env.set(">") = [](environment &env, const atom &v) {
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

atom exec(environment &env, const std::string &expr) {
    str_view sv(expr);

    tokenizer t(sv);

    atom result;

    while (t.has_next()) {
        atom parsed = build_from(t);
        result = env.eval(parsed);
    }

    return result;
}

} // namespace lispy
