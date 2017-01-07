#include <typeinfo>
#include <functional>
#include <map>

#include "rapidjson/reader.h"
#include "rapidjson/error/en.h"

#include "ace.hh"
#include "chart.hh"
#include "acmacs-base/read-file.hh"
#include "acmacs-base/xz.hh"

// ----------------------------------------------------------------------
// ~/ac/acmacs/docs/ace-format.json
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

class JsonReaderBase
{
 protected:
    class Failure : public std::runtime_error { public: using std::runtime_error::runtime_error; inline Failure() : std::runtime_error{""} {} };
    class Pop : public std::exception { public: using std::exception::exception; };
    friend class JsonEventHandler;

 public:
    virtual ~JsonReaderBase() {}

    inline virtual JsonReaderBase* StartObject() { throw Failure(typeid(*this).name() + std::string("::StartObject")); }
    inline virtual JsonReaderBase* EndObject() { throw Failure(typeid(*this).name() + std::string("::EndObject")); } // { throw Pop(); }
    inline virtual JsonReaderBase* StartArray() { throw Failure(typeid(*this).name() + std::string("::StartArray")); }
    inline virtual JsonReaderBase* EndArray() { throw Failure(typeid(*this).name() + std::string("::EndArray")); } // { throw Pop(); }
    inline virtual JsonReaderBase* Key(const char* str, rapidjson::SizeType length) { throw Failure(typeid(*this).name() + std::string("::Key \"") + std::string(str, length) + "\""); }
    inline virtual JsonReaderBase* String(const char* str, rapidjson::SizeType length) { throw Failure(typeid(*this).name() + std::string("::String \"") + std::string(str, length) + "\""); }
    inline virtual JsonReaderBase* Int(int i) { throw Failure(typeid(*this).name() + std::string("::Int ") + std::to_string(i)); }
    inline virtual JsonReaderBase* Uint(unsigned u) { throw Failure(typeid(*this).name() + std::string("::Uint ") + std::to_string(u)); }
    inline virtual JsonReaderBase* Double(double d) { throw Failure(typeid(*this).name() + std::string("::Double ") + std::to_string(d)); }
    inline virtual JsonReaderBase* Bool(bool b) { throw Failure(typeid(*this).name() + std::string("::Bool ") + std::to_string(b)); }
    inline virtual JsonReaderBase* Null() { throw Failure(typeid(*this).name() + std::string("::Null")); }
    inline virtual JsonReaderBase* Int64(int64_t i) { throw Failure(typeid(*this).name() + std::string("::Int64 ") + std::to_string(i)); }
    inline virtual JsonReaderBase* Uint64(uint64_t u) { throw Failure(typeid(*this).name() + std::string("::Uint64 ") + std::to_string(u)); }
};

// ----------------------------------------------------------------------

class JsonEventHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JsonEventHandler>
{
 private:
    template <typename... Args> inline bool handler(JsonReaderBase* (JsonReaderBase::*aHandler)(Args... args), Args... args)
        {
            try {
                auto new_handler = ((*mHandler.top()).*aHandler)(args...);
                if (new_handler)
                    mHandler.emplace(new_handler);
            }
            catch (JsonReaderBase::Pop&) {
                if (mHandler.empty())
                    return false;
                mHandler.pop();
            }
            catch (JsonReaderBase::Failure& err) {
                if (*err.what())
                    std::cerr << "ERROR: " << err.what() << std::endl;
                return false;
            }
            // catch (std::exception& err) {
            //     std::cerr << "ERROR: " << err.what() << std::endl;
            //     return false;
            // }
            return true;
        }

 public:
    template <typename Target> inline JsonEventHandler(Target& aTaget)
        {
            mHandler.emplace(json_reader(aTaget));
        }

    inline bool StartObject() { return handler(&JsonReaderBase::StartObject); }
    inline bool EndObject(rapidjson::SizeType /*memberCount*/) { return handler(&JsonReaderBase::EndObject); }
    inline bool StartArray() { return handler(&JsonReaderBase::StartArray); }
    inline bool EndArray(rapidjson::SizeType /*elementCount*/) { return handler(&JsonReaderBase::EndArray); }
    inline bool Key(const char* str, rapidjson::SizeType length, bool /*copy*/) { return handler(&JsonReaderBase::Key, str, length); }
    inline bool String(const char* str, rapidjson::SizeType length, bool /*copy*/) { return handler(&JsonReaderBase::String, str, length); }
    inline bool Int(int i) { return handler(&JsonReaderBase::Int, i); }
    inline bool Uint(unsigned u) { return handler(&JsonReaderBase::Uint, u); }
    inline bool Double(double d) { return handler(&JsonReaderBase::Double, d); }
    inline bool Bool(bool b) { return handler(&JsonReaderBase::Bool, b); }
    inline bool Null() { return handler(&JsonReaderBase::Null); }
    inline bool Int64(int64_t i) { return handler(&JsonReaderBase::Int64, i); }
    inline bool Uint64(uint64_t u) { return handler(&JsonReaderBase::Uint64, u); }

 private:
    std::stack<std::unique_ptr<JsonReaderBase>> mHandler;

}; // class JsonEventHandler<>

// ----------------------------------------------------------------------

template <typename Target> class JsonReaderObject : public JsonReaderBase
{
 public:
    inline JsonReaderObject(Target aTarget) : mTarget(aTarget), mStarted(false) {}

    inline virtual JsonReaderBase* Key(const char* str, rapidjson::SizeType length)
        {
            JsonReaderBase* r = match_key(str, length);
            if (!r) {
                  // support for keys starting or ending with ? and "_"
                r = JsonReaderBase::Key(str, length);
            }
            return r;
        }

    inline virtual JsonReaderBase* StartObject()
        {
            if (mStarted)
                throw Failure(typeid(*this).name() + std::string(": unexpected StartObject event"));
            mStarted = true;
            return nullptr;
        }

    inline virtual JsonReaderBase* EndObject()
        {
            throw Pop();
        }

 protected:
    virtual JsonReaderBase* match_key(const char* str, rapidjson::SizeType length) = 0;

    inline Target& target() { return mTarget; }

 private:
    Target mTarget;
    bool mStarted;
};

// ----------------------------------------------------------------------

// template <typename Parent, typename Field> using JsonReaderAccessorArg = decltype(std::bind(std::declval<Field& (Parent::*&)()>(), std::declval<Parent*>()));

namespace _internal
{
    template <typename F> class JsonReaderString : public JsonReaderBase
    {
     public:
        inline JsonReaderString(F aStorage) : mStorage(aStorage) {}

        inline virtual JsonReaderBase* String(const char* str, rapidjson::SizeType length)
            {
                mStorage(str, length);
                throw Pop();
            }

     private:
        F mStorage;
    };

    template <typename F> inline JsonReaderString<F>* json_reader_string(F aF) { return new JsonReaderString<F>(aF); }
}

template <typename T> inline JsonReaderBase* json_reader(void(T::*setter)(const char*, size_t), T& target)
{
    return _internal::json_reader_string(std::bind(setter, &target, std::placeholders::_1, std::placeholders::_2));
}

// for JsonReaderObject<> derivatives, e.g. return json_reader<JsonReaderChart>(&Ace::chart, target());
template <template<typename> class Reader, typename Parent, typename Field> inline JsonReaderBase* json_reader(Field& (Parent::*accessor)(), Parent& parent)
{
    using Bind = decltype(std::bind(std::declval<Field& (Parent::*&)()>(), std::declval<Parent*>()));
    return new Reader<Bind>(std::bind(accessor, &parent));
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

template <typename Parent> class JsonReaderMakerBase
{
 public:
    inline JsonReaderMakerBase() = default;
    virtual ~JsonReaderMakerBase() {}
    virtual JsonReaderBase* operator()(Parent& parent) = 0;
};

template <typename Parent, typename Func> class JsonReaderMakerSetter : public JsonReaderMakerBase<Parent>
{
 public:
    inline JsonReaderMakerSetter(Func aF) : mF(aF) {}
    virtual inline JsonReaderBase* operator()(Parent& parent) { return json_reader(mF, parent); }

 private:
    Func mF;
};

template <template<typename> class Reader, typename Parent, typename Func> class JsonReaderMakerAccessor : public JsonReaderMakerBase<Parent>
{
 public:
    inline JsonReaderMakerAccessor(Func aF) : mF(aF) {}
    virtual inline JsonReaderBase* operator()(Parent& parent) { return json_reader<Reader>(mF, parent); }

 private:
    Func mF;
};

template <typename Parent> class JsonReaderMakerWrapper
{
 public:
    inline JsonReaderMakerWrapper(JsonReaderMakerBase<Parent>* aMaker) : mMaker(aMaker) {}
    inline JsonReaderBase* operator()(Parent& parent) { return (*mMaker)(parent); }

 private:
    std::shared_ptr<JsonReaderMakerBase<Parent>> mMaker; // cannot have unique_ptr here because std::map requires copying
};

template <typename Parent> using JsonReaderData = std::map<std::string,JsonReaderMakerWrapper<Parent>>;

template <typename Parent, typename ...Args> inline JsonReaderMakerBase<Parent>* json_reader_maker(void (Parent::*setter)(Args...))
{
    return new JsonReaderMakerSetter<Parent, decltype(setter)>(setter);
}

template <template<typename> class Reader, typename Parent, typename Field> inline JsonReaderMakerBase<Parent>* json_reader_maker(Field& (Parent::*accessor)())
{
    return new JsonReaderMakerAccessor<Reader, Parent, decltype(accessor)>(accessor);
}

template <typename Target> class JsonReaderForDataRef : public JsonReaderObject<Target&>
{
 public:
    inline JsonReaderForDataRef(Target& aTarget, JsonReaderData<Target>& aData) : JsonReaderObject<Target&>(aTarget), mData(aData) {}

 protected:
    virtual inline JsonReaderBase* match_key(const char* str, rapidjson::SizeType length)
        {
            const std::string k{str, length};
            std::cerr << typeid(*this).name() << " " << k << std::endl;
            auto e = mData.find(k);
            if (e != mData.end())
                return e->second(this->target());
            return nullptr;
        }

 private:
    JsonReaderData<Target>& mData;

}; // class JsonReaderAce

template <typename Target> inline JsonReaderForDataRef<Target>* json_reader_for_data(Target& aTarget, JsonReaderData<Target>& aData)
{
    return new JsonReaderForDataRef<Target>(aTarget, aData);
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

class Ace
{
 public:
    inline Ace(Chart& aChar) : mChart(aChar) {}

    inline void indentation(const char* str, size_t length) { std::cerr << "Indentation: " << std::string(str, length) << std::endl; mIndentation.assign(str, length); }
    inline std::string indentation() const { return mIndentation; }
    inline void version(const char* str, size_t length)
        {
            mVersion.assign(str, length);
            if (mVersion != "acmacs-ace-v1")
                throw std::runtime_error("Unsupported data version: \"" + mVersion + "\"");
        }
    inline std::string version() const { return mVersion; }

    inline Chart& chart() { return mChart; }

 private:
    std::string mIndentation;
    std::string mVersion;
    Chart& mChart;
};

// ----------------------------------------------------------------------

template <typename Accessor> class JsonReaderChart : public JsonReaderObject<Accessor>
{
 public:
    using JsonReaderObject<Accessor>::JsonReaderObject;

 protected:
    virtual inline JsonReaderBase* match_key(const char* str, rapidjson::SizeType length)
        {
            const std::string k{str, length};
            std::cerr << "JsonReaderChart " << k << std::endl;
            return nullptr;
        }

}; // class JsonReaderChart

// ----------------------------------------------------------------------

// class JsonReaderAce : public JsonReaderObject<Ace&>
// {
//  public:
//     using JsonReaderObject<Ace&>::JsonReaderObject;

//  protected:
//     virtual inline JsonReaderBase* match_key(const char* str, rapidjson::SizeType length)
//         {
//             const std::string k{str, length};
//             if (k == "_")
//                 return json_reader(&Ace::indentation, target());
//             else if (k == "  version")
//                 return json_reader(&Ace::version, target());
//             else if (k == "c")
//                 return json_reader<JsonReaderChart>(&Ace::chart, target());
//             return nullptr;
//         }
// }; // class JsonReaderAce

static JsonReaderData<Ace> ace_reader_maker_data = {
//static std::map<std::string,JsonReaderMakerBase<Ace>*> ace_reader_maker_data = {
    {"_", json_reader_maker(&Ace::indentation)},
    {"  version", json_reader_maker(&Ace::version)},
    {"c", json_reader_maker<JsonReaderChart>(&Ace::chart)},
};

inline JsonReaderBase* json_reader(Ace& target)
{
    // return new JsonReaderAce(target);
    return json_reader_for_data(target, ace_reader_maker_data);
}

// ----------------------------------------------------------------------

Chart* import_chart(std::string buffer)
{
    if (buffer == "-")
        buffer = acmacs_base::read_stdin();
    else if (buffer[0] != '{')
        buffer = acmacs_base::read_file(buffer);
    Chart* chart = nullptr;
    if (buffer[0] == '{') { // && buffer.find("\"  version\": \"acmacs-ace-v1\"") != std::string::npos) {
        chart = new Chart{};
        Ace ace(*chart);
        JsonEventHandler handler{ace};
        rapidjson::Reader reader;
        rapidjson::StringStream ss(buffer.c_str());
        reader.Parse(ss, handler);
        if (reader.HasParseError())
            throw std::runtime_error("cannot import chart: data parsing failed at " + std::to_string(reader.GetErrorOffset()) + ": " +  GetParseError_En(reader.GetParseErrorCode()));
    }
    else
        throw std::runtime_error("cannot import chart: unrecognized source format");
    return chart;

} // import_chart

// ----------------------------------------------------------------------
/// Local Variables:
/// eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
/// End:
