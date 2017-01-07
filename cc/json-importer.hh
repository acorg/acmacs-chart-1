#pragma once

#include "rapidjson/reader.h"
#include "rapidjson/error/en.h"

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

template <typename Target> class JsonReaderObject : public JsonReaderBase
{
 public:
    inline JsonReaderObject(Target aTarget) : mTarget(aTarget), mStarted(false) {}

    inline virtual JsonReaderBase* Key(const char* str, rapidjson::SizeType length)
        {
              // std::cerr << "JsonReaderObject::Key " << std::string(str, length) << std::endl;
            JsonReaderBase* r = match_key(str, length);
            if (!r) {
                  // support for keys starting or ending with ? and "_"
                r = JsonReaderBase::Key(str, length);
            }
            // else
            //     std::cerr << "JsonReaderObject " << std::string(str, length) << " -> PUSH " << typeid(*r).name() << std::endl;
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
    virtual JsonReaderBase* reader(Parent& parent) = 0;
};

template <typename Parent, typename Func> class JsonReaderMakerSetter : public JsonReaderMakerBase<Parent>
{
 public:
    inline JsonReaderMakerSetter(Func aF) : mF(aF) {}
    virtual inline JsonReaderBase* reader(Parent& parent) { return json_reader(mF, parent); }

 private:
    Func mF;
};

template <typename Parent, typename ...Args> inline std::shared_ptr<JsonReaderMakerBase<Parent>> json_reader_maker(void (Parent::*setter)(Args...))
{
    return std::make_shared<JsonReaderMakerSetter<Parent, decltype(setter)>>(setter);
}

// ----------------------------------------------------------------------

template <typename Parent> using JsonReaderData = std::map<std::string,std::shared_ptr<JsonReaderMakerBase<Parent>>>; // cannot have unique_ptr here because std::map requires copying

template <typename Target> class JsonReaderForDataRef : public JsonReaderObject<Target&>
{
 public:
    inline JsonReaderForDataRef(Target& aTarget, JsonReaderData<Target>& aData) : JsonReaderObject<Target&>(aTarget), mData(aData) {}

 protected:
    virtual inline JsonReaderBase* match_key(const char* str, rapidjson::SizeType length)
        {
            const std::string k{str, length};
            // std::cerr << typeid(*this).name() << " " << k << std::endl;
            auto e = mData.find(k);
            if (e != mData.end())
                  // return e->second(this->target());
                return e->second->reader(this->target());
            return nullptr;
        }

 private:
    JsonReaderData<Target>& mData;

}; // class JsonReaderForDataRef<Target>

// ----------------------------------------------------------------------

// template <typename Target, typename Accessor> class JsonReaderForDataAccessor : public JsonReaderObject<Accessor>
// {
//  public:
//     inline JsonReaderForDataAccessor(Accessor aAccessor, JsonReaderData<Target>& aData) : JsonReaderObject<Accessor>(aAccessor), mData(aData) {}

//  protected:
//     virtual inline JsonReaderBase* match_key(const char* str, rapidjson::SizeType length)
//         {
//             const std::string k{str, length};
//             // std::cerr << typeid(*this).name() << " " << k << std::endl;
//             auto e = mData.find(k);
//             if (e != mData.end()) {
//                 return e->second(this->target()());
//             }
//             return nullptr;
//         }

//  private:
//     JsonReaderData<Target>& mData;

// }; // class JsonReaderForDataAccessor<Target, Accessor>

// template <typename Parent, typename Field, typename Func> class JsonReaderMakerAccessor : public JsonReaderMakerBase<Parent>
// {
//  public:
//     inline JsonReaderMakerAccessor(Func aF, JsonReaderData<Field>& aData) : mF(aF), mData(aData) {}
//     virtual inline JsonReaderBase* operator()(Parent& parent)
//         {
//             using Bind = decltype(std::bind(mF, &parent));
//             return new JsonReaderForDataAccessor<Field, Bind>(std::bind(mF, &parent), mData);
//         }

//  private:
//     Func mF;
//     JsonReaderData<Field>& mData;
// };

template <typename Parent, typename Field, typename Func> class JsonReaderMakerAccessor : public JsonReaderMakerBase<Parent>
{
 public:
    inline JsonReaderMakerAccessor(Func aF, JsonReaderData<Field>& aData) : mF(aF), mData(aData) {}
    virtual inline JsonReaderBase* reader(Parent& parent)
        {
            return new JsonReaderForDataRef<Field>(std::bind(mF, &parent)(), mData);
        }

 private:
    Func mF;
    JsonReaderData<Field>& mData;
};

template <typename Parent, typename Field> inline std::shared_ptr<JsonReaderMakerBase<Parent>> json_reader_maker(Field& (Parent::*accessor)(), JsonReaderData<Field>& data)
{
    return std::make_shared<JsonReaderMakerAccessor<Parent, Field, decltype(accessor)>>(accessor, data);
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

class JsonEventHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JsonEventHandler>
{
 public:
    // template <typename Target> inline JsonEventHandler(Target& aTarget)
    //     {
    //         mHandler.emplace(json_reader(aTarget));
    //     }

    template <typename Target> inline JsonEventHandler(Target& aTarget, JsonReaderData<Target>& aData)
        {
            mHandler.emplace(new JsonReaderForDataRef<Target>(aTarget, aData));
        }

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
/// Local Variables:
/// eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
/// End:
