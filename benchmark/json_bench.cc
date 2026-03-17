/**
 * @file json_bench.cc
 * @brief Comprehensive JSON library benchmark comparing json-gen-c against
 *        cJSON, yyjson, jansson, json-c, and rapidjson.
 *
 * jansson and json-c have conflicting type names (json_object, json_type),
 * so their benchmarks live in separate translation units.
 */

#include <benchmark/benchmark.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "json.gen.h"
}

#include <cjson/cJSON.h>
#include <yyjson.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

extern void register_jansson_benchmarks();
extern void register_jsonc_benchmarks();

extern "C" {
extern const char BENCH_JSON_SCALAR[] =
    "{\"int_val1\":1,\"int_val2\":1,\"long_val\":2,"
    "\"double_val\":3.0,\"float_val\":4.0,"
    "\"sstr_val\":\"hello this is string\"}";

extern const char BENCH_JSON_NESTED[] =
    "{\"name\":\"John Doe\",\"age\":42,"
    "\"home_addr\":{\"street\":\"123 Main St\",\"city\":\"Springfield\","
    "\"state\":\"IL\",\"zip\":\"62701\"},"
    "\"work_addr\":{\"street\":\"456 Oak Ave\",\"city\":\"Chicago\","
    "\"state\":\"IL\",\"zip\":\"60601\"}}";

extern const char BENCH_JSON_STRING_HEAVY[] =
    "{\"first_name\":\"Alexander\",\"last_name\":\"Constantinovich\","
    "\"email\":\"alexander.constantinovich@example.com\","
    "\"phone\":\"+1-555-123-4567\","
    "\"bio\":\"A software engineer with over 15 years of experience "
    "in distributed systems and compiler design.\","
    "\"website\":\"https://example.com/alexander\","
    "\"company\":\"Acme Corporation International\","
    "\"title\":\"Principal Software Engineer\"}";
}

static void init_address(struct address* a, const char* street, const char* city,
                         const char* st, const char* zip) {
    address_init(a); a->street=sstr(street); a->city=sstr(city);
    a->state=sstr(st); a->zip=sstr(zip);
}
static void fill_scalar(struct scalar* o) {
    scalar_init(o); o->int_val1=1; o->int_val2=1; o->long_val=2;
    o->double_val=3.0; o->float_val=4.0; o->sstr_val=sstr("hello this is string");
}
static void fill_nested(struct nested* o) {
    nested_init(o); o->name=sstr("John Doe"); o->age=42;
    init_address(&o->home_addr,"123 Main St","Springfield","IL","62701");
    init_address(&o->work_addr,"456 Oak Ave","Chicago","IL","60601");
}
static void fill_string_heavy(struct string_heavy* o) {
    string_heavy_init(o);
    o->first_name=sstr("Alexander"); o->last_name=sstr("Constantinovich");
    o->email=sstr("alexander.constantinovich@example.com");
    o->phone=sstr("+1-555-123-4567");
    o->bio=sstr("A software engineer with over 15 years of experience in distributed systems and compiler design.");
    o->website=sstr("https://example.com/alexander");
    o->company=sstr("Acme Corporation International");
    o->title=sstr("Principal Software Engineer");
}

/* ---- json-gen-c ---- */
static void BM_jgenc_marshal_scalar(benchmark::State& s){struct scalar o;fill_scalar(&o);sstr_t out=sstr_new();size_t t=0;for(auto _:s){json_marshal_scalar(&o,out);t+=sstr_length(out);sstr_clear(out);}s.SetBytesProcessed((int64_t)t);sstr_free(out);scalar_clear(&o);}
static void BM_jgenc_unmarshal_scalar(benchmark::State& s){struct scalar o;fill_scalar(&o);sstr_t c=sstr_new();json_marshal_scalar(&o,c);size_t t=0;for(auto _:s){scalar_clear(&o);json_unmarshal_scalar(c,&o);t+=sstr_length(c);}s.SetBytesProcessed((int64_t)t);sstr_free(c);scalar_clear(&o);}
static void BM_jgenc_marshal_nested(benchmark::State& s){struct nested o;fill_nested(&o);sstr_t out=sstr_new();size_t t=0;for(auto _:s){json_marshal_nested(&o,out);t+=sstr_length(out);sstr_clear(out);}s.SetBytesProcessed((int64_t)t);sstr_free(out);nested_clear(&o);}
static void BM_jgenc_unmarshal_nested(benchmark::State& s){struct nested o;fill_nested(&o);sstr_t c=sstr_new();json_marshal_nested(&o,c);size_t t=0;for(auto _:s){nested_clear(&o);json_unmarshal_nested(c,&o);t+=sstr_length(c);}s.SetBytesProcessed((int64_t)t);sstr_free(c);nested_clear(&o);}
static void BM_jgenc_unmarshal_nested_selected(benchmark::State& s){struct nested seed;fill_nested(&seed);sstr_t c=sstr_new();json_marshal_nested(&seed,c);uint64_t m[nested_FIELD_MASK_WORD_COUNT]={0};JSON_GEN_C_FIELD_MASK_SET(m,nested_FIELD_name);JSON_GEN_C_FIELD_MASK_SET(m,nested_FIELD_age);size_t t=0;for(auto _:s){struct nested o;nested_init(&o);json_unmarshal_selected_nested(c,&o,m,nested_FIELD_MASK_WORD_COUNT);t+=sstr_length(c);nested_clear(&o);}s.SetBytesProcessed((int64_t)t);sstr_free(c);nested_clear(&seed);}
static void BM_jgenc_marshal_string_heavy(benchmark::State& s){struct string_heavy o;fill_string_heavy(&o);sstr_t out=sstr_new();size_t t=0;for(auto _:s){json_marshal_string_heavy(&o,out);t+=sstr_length(out);sstr_clear(out);}s.SetBytesProcessed((int64_t)t);sstr_free(out);string_heavy_clear(&o);}
static void BM_jgenc_unmarshal_string_heavy(benchmark::State& s){struct string_heavy o;fill_string_heavy(&o);sstr_t c=sstr_new();json_marshal_string_heavy(&o,c);size_t t=0;for(auto _:s){string_heavy_clear(&o);json_unmarshal_string_heavy(c,&o);t+=sstr_length(c);}s.SetBytesProcessed((int64_t)t);sstr_free(c);string_heavy_clear(&o);}
static void BM_jgenc_unmarshal_string_heavy_selected(benchmark::State& s){struct string_heavy seed;fill_string_heavy(&seed);sstr_t c=sstr_new();json_marshal_string_heavy(&seed,c);uint64_t m[string_heavy_FIELD_MASK_WORD_COUNT]={0};JSON_GEN_C_FIELD_MASK_SET(m,string_heavy_FIELD_email);JSON_GEN_C_FIELD_MASK_SET(m,string_heavy_FIELD_phone);size_t t=0;for(auto _:s){struct string_heavy o;string_heavy_init(&o);json_unmarshal_selected_string_heavy(c,&o,m,string_heavy_FIELD_MASK_WORD_COUNT);t+=sstr_length(c);string_heavy_clear(&o);}s.SetBytesProcessed((int64_t)t);sstr_free(c);string_heavy_clear(&seed);}
static void BM_jgenc_marshal_scalar_array(benchmark::State& s){int n=(int)s.range(0);auto*obj=new struct scalar[(size_t)n];for(int i=0;i<n;i++)fill_scalar(&obj[i]);sstr_t out=sstr_new();size_t t=0;for(auto _:s){json_marshal_array_scalar(obj,n,out);t+=sstr_length(out);sstr_clear(out);}s.SetBytesProcessed((int64_t)t);sstr_free(out);for(int i=0;i<n;i++)scalar_clear(&obj[i]);delete[]obj;}
static void BM_jgenc_unmarshal_scalar_array(benchmark::State& s){int n=(int)s.range(0);auto*obj=new struct scalar[(size_t)n];for(int i=0;i<n;i++)fill_scalar(&obj[i]);sstr_t c=sstr_new();json_marshal_array_scalar(obj,n,c);for(int i=0;i<n;i++)scalar_clear(&obj[i]);delete[]obj;size_t t=0;for(auto _:s){struct scalar*r=NULL;int rn=0;json_unmarshal_array_scalar(c,&r,&rn);t+=sstr_length(c);for(int i=0;i<rn;i++)scalar_clear(&r[i]);free(r);}s.SetBytesProcessed((int64_t)t);sstr_free(c);}

/* ---- cJSON ---- */
static void BM_cjson_marshal_scalar(benchmark::State& s){cJSON*r=cJSON_CreateObject();cJSON_AddNumberToObject(r,"int_val1",1);cJSON_AddNumberToObject(r,"int_val2",1);cJSON_AddNumberToObject(r,"long_val",2);cJSON_AddNumberToObject(r,"double_val",3.0);cJSON_AddNumberToObject(r,"float_val",4.0);cJSON_AddStringToObject(r,"sstr_val","hello this is string");size_t t=0;for(auto _:s){char*o=cJSON_PrintUnformatted(r);t+=strlen(o);cJSON_free(o);}s.SetBytesProcessed((int64_t)t);cJSON_Delete(r);}
static void BM_cjson_unmarshal_scalar(benchmark::State& s){size_t l=strlen(BENCH_JSON_SCALAR),t=0;for(auto _:s){cJSON*r=cJSON_Parse(BENCH_JSON_SCALAR);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"int_val1")->valueint);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"int_val2")->valueint);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"long_val")->valueint);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"double_val")->valuedouble);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"float_val")->valuedouble);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"sstr_val")->valuestring);t+=l;cJSON_Delete(r);}s.SetBytesProcessed((int64_t)t);}
static void BM_cjson_marshal_nested(benchmark::State& s){cJSON*r=cJSON_CreateObject();cJSON_AddStringToObject(r,"name","John Doe");cJSON_AddNumberToObject(r,"age",42);cJSON*h=cJSON_AddObjectToObject(r,"home_addr");cJSON_AddStringToObject(h,"street","123 Main St");cJSON_AddStringToObject(h,"city","Springfield");cJSON_AddStringToObject(h,"state","IL");cJSON_AddStringToObject(h,"zip","62701");cJSON*w=cJSON_AddObjectToObject(r,"work_addr");cJSON_AddStringToObject(w,"street","456 Oak Ave");cJSON_AddStringToObject(w,"city","Chicago");cJSON_AddStringToObject(w,"state","IL");cJSON_AddStringToObject(w,"zip","60601");size_t t=0;for(auto _:s){char*o=cJSON_PrintUnformatted(r);t+=strlen(o);cJSON_free(o);}s.SetBytesProcessed((int64_t)t);cJSON_Delete(r);}
static void BM_cjson_unmarshal_nested(benchmark::State& s){size_t l=strlen(BENCH_JSON_NESTED),t=0;for(auto _:s){cJSON*r=cJSON_Parse(BENCH_JSON_NESTED);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"name")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"age")->valueint);cJSON*h=cJSON_GetObjectItem(r,"home_addr");benchmark::DoNotOptimize(cJSON_GetObjectItem(h,"street")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(h,"city")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(h,"state")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(h,"zip")->valuestring);cJSON*w=cJSON_GetObjectItem(r,"work_addr");benchmark::DoNotOptimize(cJSON_GetObjectItem(w,"street")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(w,"city")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(w,"state")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(w,"zip")->valuestring);t+=l;cJSON_Delete(r);}s.SetBytesProcessed((int64_t)t);}
static void BM_cjson_marshal_string_heavy(benchmark::State& s){cJSON*r=cJSON_CreateObject();cJSON_AddStringToObject(r,"first_name","Alexander");cJSON_AddStringToObject(r,"last_name","Constantinovich");cJSON_AddStringToObject(r,"email","alexander.constantinovich@example.com");cJSON_AddStringToObject(r,"phone","+1-555-123-4567");cJSON_AddStringToObject(r,"bio","A software engineer with over 15 years of experience in distributed systems and compiler design.");cJSON_AddStringToObject(r,"website","https://example.com/alexander");cJSON_AddStringToObject(r,"company","Acme Corporation International");cJSON_AddStringToObject(r,"title","Principal Software Engineer");size_t t=0;for(auto _:s){char*o=cJSON_PrintUnformatted(r);t+=strlen(o);cJSON_free(o);}s.SetBytesProcessed((int64_t)t);cJSON_Delete(r);}
static void BM_cjson_unmarshal_string_heavy(benchmark::State& s){size_t l=strlen(BENCH_JSON_STRING_HEAVY),t=0;for(auto _:s){cJSON*r=cJSON_Parse(BENCH_JSON_STRING_HEAVY);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"first_name")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"last_name")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"email")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"phone")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"bio")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"website")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"company")->valuestring);benchmark::DoNotOptimize(cJSON_GetObjectItem(r,"title")->valuestring);t+=l;cJSON_Delete(r);}s.SetBytesProcessed((int64_t)t);}

/* ---- yyjson ---- */
static void BM_yyjson_marshal_scalar(benchmark::State& s){size_t t=0;for(auto _:s){yyjson_mut_doc*d=yyjson_mut_doc_new(NULL);yyjson_mut_val*r=yyjson_mut_obj(d);yyjson_mut_doc_set_root(d,r);yyjson_mut_obj_add_int(d,r,"int_val1",1);yyjson_mut_obj_add_int(d,r,"int_val2",1);yyjson_mut_obj_add_int(d,r,"long_val",2);yyjson_mut_obj_add_real(d,r,"double_val",3.0);yyjson_mut_obj_add_real(d,r,"float_val",4.0);yyjson_mut_obj_add_str(d,r,"sstr_val","hello this is string");size_t jl;char*j=yyjson_mut_write(d,0,&jl);t+=jl;free(j);yyjson_mut_doc_free(d);}s.SetBytesProcessed((int64_t)t);}
static void BM_yyjson_unmarshal_scalar(benchmark::State& s){size_t l=strlen(BENCH_JSON_SCALAR),t=0;for(auto _:s){yyjson_doc*d=yyjson_read(BENCH_JSON_SCALAR,l,0);yyjson_val*r=yyjson_doc_get_root(d);benchmark::DoNotOptimize(yyjson_get_int(yyjson_obj_get(r,"int_val1")));benchmark::DoNotOptimize(yyjson_get_int(yyjson_obj_get(r,"int_val2")));benchmark::DoNotOptimize(yyjson_get_sint(yyjson_obj_get(r,"long_val")));benchmark::DoNotOptimize(yyjson_get_real(yyjson_obj_get(r,"double_val")));benchmark::DoNotOptimize(yyjson_get_real(yyjson_obj_get(r,"float_val")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"sstr_val")));t+=l;yyjson_doc_free(d);}s.SetBytesProcessed((int64_t)t);}
static void BM_yyjson_marshal_nested(benchmark::State& s){size_t t=0;for(auto _:s){yyjson_mut_doc*d=yyjson_mut_doc_new(NULL);yyjson_mut_val*r=yyjson_mut_obj(d);yyjson_mut_doc_set_root(d,r);yyjson_mut_obj_add_str(d,r,"name","John Doe");yyjson_mut_obj_add_int(d,r,"age",42);yyjson_mut_val*h=yyjson_mut_obj(d);yyjson_mut_obj_add_val(d,r,"home_addr",h);yyjson_mut_obj_add_str(d,h,"street","123 Main St");yyjson_mut_obj_add_str(d,h,"city","Springfield");yyjson_mut_obj_add_str(d,h,"state","IL");yyjson_mut_obj_add_str(d,h,"zip","62701");yyjson_mut_val*w=yyjson_mut_obj(d);yyjson_mut_obj_add_val(d,r,"work_addr",w);yyjson_mut_obj_add_str(d,w,"street","456 Oak Ave");yyjson_mut_obj_add_str(d,w,"city","Chicago");yyjson_mut_obj_add_str(d,w,"state","IL");yyjson_mut_obj_add_str(d,w,"zip","60601");size_t jl;char*j=yyjson_mut_write(d,0,&jl);t+=jl;free(j);yyjson_mut_doc_free(d);}s.SetBytesProcessed((int64_t)t);}
static void BM_yyjson_unmarshal_nested(benchmark::State& s){size_t l=strlen(BENCH_JSON_NESTED),t=0;for(auto _:s){yyjson_doc*d=yyjson_read(BENCH_JSON_NESTED,l,0);yyjson_val*r=yyjson_doc_get_root(d);benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"name")));benchmark::DoNotOptimize(yyjson_get_int(yyjson_obj_get(r,"age")));yyjson_val*h=yyjson_obj_get(r,"home_addr");benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(h,"street")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(h,"city")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(h,"state")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(h,"zip")));yyjson_val*w=yyjson_obj_get(r,"work_addr");benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(w,"street")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(w,"city")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(w,"state")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(w,"zip")));t+=l;yyjson_doc_free(d);}s.SetBytesProcessed((int64_t)t);}
static void BM_yyjson_marshal_string_heavy(benchmark::State& s){size_t t=0;for(auto _:s){yyjson_mut_doc*d=yyjson_mut_doc_new(NULL);yyjson_mut_val*r=yyjson_mut_obj(d);yyjson_mut_doc_set_root(d,r);yyjson_mut_obj_add_str(d,r,"first_name","Alexander");yyjson_mut_obj_add_str(d,r,"last_name","Constantinovich");yyjson_mut_obj_add_str(d,r,"email","alexander.constantinovich@example.com");yyjson_mut_obj_add_str(d,r,"phone","+1-555-123-4567");yyjson_mut_obj_add_str(d,r,"bio","A software engineer with over 15 years of experience in distributed systems and compiler design.");yyjson_mut_obj_add_str(d,r,"website","https://example.com/alexander");yyjson_mut_obj_add_str(d,r,"company","Acme Corporation International");yyjson_mut_obj_add_str(d,r,"title","Principal Software Engineer");size_t jl;char*j=yyjson_mut_write(d,0,&jl);t+=jl;free(j);yyjson_mut_doc_free(d);}s.SetBytesProcessed((int64_t)t);}
static void BM_yyjson_unmarshal_string_heavy(benchmark::State& s){size_t l=strlen(BENCH_JSON_STRING_HEAVY),t=0;for(auto _:s){yyjson_doc*d=yyjson_read(BENCH_JSON_STRING_HEAVY,l,0);yyjson_val*r=yyjson_doc_get_root(d);benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"first_name")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"last_name")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"email")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"phone")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"bio")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"website")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"company")));benchmark::DoNotOptimize(yyjson_get_str(yyjson_obj_get(r,"title")));t+=l;yyjson_doc_free(d);}s.SetBytesProcessed((int64_t)t);}

/* ---- rapidjson ---- */
static void BM_rapidjson_marshal_scalar(benchmark::State& s){size_t t=0;for(auto _:s){rapidjson::StringBuffer sb;rapidjson::Writer<rapidjson::StringBuffer> w(sb);w.StartObject();w.Key("int_val1");w.Int(1);w.Key("int_val2");w.Int(1);w.Key("long_val");w.Int64(2);w.Key("double_val");w.Double(3.0);w.Key("float_val");w.Double(4.0);w.Key("sstr_val");w.String("hello this is string");w.EndObject();t+=sb.GetSize();}s.SetBytesProcessed((int64_t)t);}
static void BM_rapidjson_unmarshal_scalar(benchmark::State& s){size_t l=strlen(BENCH_JSON_SCALAR),t=0;for(auto _:s){rapidjson::Document d;d.Parse(BENCH_JSON_SCALAR);benchmark::DoNotOptimize(d["int_val1"].GetInt());benchmark::DoNotOptimize(d["int_val2"].GetInt());benchmark::DoNotOptimize(d["long_val"].GetInt64());benchmark::DoNotOptimize(d["double_val"].GetDouble());benchmark::DoNotOptimize(d["float_val"].GetDouble());benchmark::DoNotOptimize(d["sstr_val"].GetString());t+=l;}s.SetBytesProcessed((int64_t)t);}
static void BM_rapidjson_marshal_nested(benchmark::State& s){size_t t=0;for(auto _:s){rapidjson::StringBuffer sb;rapidjson::Writer<rapidjson::StringBuffer> w(sb);w.StartObject();w.Key("name");w.String("John Doe");w.Key("age");w.Int(42);w.Key("home_addr");w.StartObject();w.Key("street");w.String("123 Main St");w.Key("city");w.String("Springfield");w.Key("state");w.String("IL");w.Key("zip");w.String("62701");w.EndObject();w.Key("work_addr");w.StartObject();w.Key("street");w.String("456 Oak Ave");w.Key("city");w.String("Chicago");w.Key("state");w.String("IL");w.Key("zip");w.String("60601");w.EndObject();w.EndObject();t+=sb.GetSize();}s.SetBytesProcessed((int64_t)t);}
static void BM_rapidjson_unmarshal_nested(benchmark::State& s){size_t l=strlen(BENCH_JSON_NESTED),t=0;for(auto _:s){rapidjson::Document d;d.Parse(BENCH_JSON_NESTED);benchmark::DoNotOptimize(d["name"].GetString());benchmark::DoNotOptimize(d["age"].GetInt());const auto&h=d["home_addr"];benchmark::DoNotOptimize(h["street"].GetString());benchmark::DoNotOptimize(h["city"].GetString());benchmark::DoNotOptimize(h["state"].GetString());benchmark::DoNotOptimize(h["zip"].GetString());const auto&w2=d["work_addr"];benchmark::DoNotOptimize(w2["street"].GetString());benchmark::DoNotOptimize(w2["city"].GetString());benchmark::DoNotOptimize(w2["state"].GetString());benchmark::DoNotOptimize(w2["zip"].GetString());t+=l;}s.SetBytesProcessed((int64_t)t);}
static void BM_rapidjson_marshal_string_heavy(benchmark::State& s){size_t t=0;for(auto _:s){rapidjson::StringBuffer sb;rapidjson::Writer<rapidjson::StringBuffer> w(sb);w.StartObject();w.Key("first_name");w.String("Alexander");w.Key("last_name");w.String("Constantinovich");w.Key("email");w.String("alexander.constantinovich@example.com");w.Key("phone");w.String("+1-555-123-4567");w.Key("bio");w.String("A software engineer with over 15 years of experience in distributed systems and compiler design.");w.Key("website");w.String("https://example.com/alexander");w.Key("company");w.String("Acme Corporation International");w.Key("title");w.String("Principal Software Engineer");w.EndObject();t+=sb.GetSize();}s.SetBytesProcessed((int64_t)t);}
static void BM_rapidjson_unmarshal_string_heavy(benchmark::State& s){size_t l=strlen(BENCH_JSON_STRING_HEAVY),t=0;for(auto _:s){rapidjson::Document d;d.Parse(BENCH_JSON_STRING_HEAVY);benchmark::DoNotOptimize(d["first_name"].GetString());benchmark::DoNotOptimize(d["last_name"].GetString());benchmark::DoNotOptimize(d["email"].GetString());benchmark::DoNotOptimize(d["phone"].GetString());benchmark::DoNotOptimize(d["bio"].GetString());benchmark::DoNotOptimize(d["website"].GetString());benchmark::DoNotOptimize(d["company"].GetString());benchmark::DoNotOptimize(d["title"].GetString());t+=l;}s.SetBytesProcessed((int64_t)t);}

/* ---- register ---- */
BENCHMARK(BM_jgenc_marshal_scalar); BENCHMARK(BM_jgenc_unmarshal_scalar);
BENCHMARK(BM_jgenc_marshal_nested); BENCHMARK(BM_jgenc_unmarshal_nested);
BENCHMARK(BM_jgenc_unmarshal_nested_selected);
BENCHMARK(BM_jgenc_marshal_string_heavy); BENCHMARK(BM_jgenc_unmarshal_string_heavy);
BENCHMARK(BM_jgenc_unmarshal_string_heavy_selected);
BENCHMARK(BM_jgenc_marshal_scalar_array)->Args({64});
BENCHMARK(BM_jgenc_unmarshal_scalar_array)->Args({64});
BENCHMARK(BM_cjson_marshal_scalar); BENCHMARK(BM_cjson_unmarshal_scalar);
BENCHMARK(BM_cjson_marshal_nested); BENCHMARK(BM_cjson_unmarshal_nested);
BENCHMARK(BM_cjson_marshal_string_heavy); BENCHMARK(BM_cjson_unmarshal_string_heavy);
BENCHMARK(BM_yyjson_marshal_scalar); BENCHMARK(BM_yyjson_unmarshal_scalar);
BENCHMARK(BM_yyjson_marshal_nested); BENCHMARK(BM_yyjson_unmarshal_nested);
BENCHMARK(BM_yyjson_marshal_string_heavy); BENCHMARK(BM_yyjson_unmarshal_string_heavy);
BENCHMARK(BM_rapidjson_marshal_scalar); BENCHMARK(BM_rapidjson_unmarshal_scalar);
BENCHMARK(BM_rapidjson_marshal_nested); BENCHMARK(BM_rapidjson_unmarshal_nested);
BENCHMARK(BM_rapidjson_marshal_string_heavy); BENCHMARK(BM_rapidjson_unmarshal_string_heavy);

int main(int argc, char** argv) {
    register_jansson_benchmarks();
    register_jsonc_benchmarks();
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
