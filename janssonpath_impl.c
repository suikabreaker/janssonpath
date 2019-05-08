#include <ctype.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>

#include "janssonpath.h"
#include "parse.h"

#define TRUE 1
#define FALSE 0

#define NEW(type) ((type *)malloc(sizeof(type)))
#define DELETE(obj) free(obj)

// all static functions here are internal implement. they follow the convention
// below: no check of arguments - pname/path must not be NULL; no accepting root
// note '$'; deal with [path, end) or from path till NULL encountered if
// end==NULL note that almost all jansson functions work fine with NULL input

static json_t *json_path_get_all_properties(json_t *json) {
  if (json_is_array(json))
    return json_incref(json);
  else if (json_is_object(json)) {
    json_t *ret = json_array();
    json_t *value;
    const char *key;
    json_object_foreach(json, key, value) { json_array_append(ret, value); }
    return ret;
  }
  else return NULL;
}

//remember to clean up stuffs before fail()
//note it can be recoverable fail - like property sellect on non-object when dealing with colleciton
#define fail() return result;

typedef enum logic_operator{
  logic_positive,
  logic_negtive,
  logic_and,
  logic_or
}logic_operator;

typedef enum comparation_operator{
  comparation_nocmp,
  comparation_eq,
  comparation_ne,
  comparation_gt,
  comparation_ge,
  comparation_lt,
  comparation_le,
  comparation_reg,//yet not supported
  comparation_error
}comparation_operator;

typedef struct sub_path{//sub_path does not own the string
  const char* start;
  const char* end;
}sub_path;

typedef struct comparation{
  comparation_operator operator;
  sub_path oprand[2];//number of oprand is defined by operator
}comparation;


struct logic_exp;//forward declartion
typedef struct expression{
  union{
    struct logic_exp* logic;
    comparation* comp;
  };
  enum tag_t{
    exp_logic, exp_compare, exp_error
  }tag;
} expression;

typedef struct logic_exp{
  logic_operator operator;
  expression oprand[2];//number of oprand is defined by operator
} logic_exp;

// static json_t* eval_sub(json_t *root, json_t * curr, expression* exp){
//   return NULL;
// }

static void remove_out_most_bar(const char ** start, const char** end){
  while((*start)[0]=='('&&(*end)[-1]==')'){
    (*start)++;(*end)--;
  }
}

static comparation* compile_compare(const char * start, const char* end){
  remove_out_most_bar(&start,&end);
  comparation* result=NULL;
  const char* comparation_first=jassonpath_next_punctors_outside_para(start,end,"=<>!");
  result=NEW(comparation);
  if(comparation_first==end||!comparation_first[0]){
    result->operator=comparation_nocmp;
    result->oprand[0].start=start;
    result->oprand[0].start=end;
    return result;
  }
  int len;
  //technically uesr can do something like (@.a>@.b)==@.t
  //however we ignore it for now
  //simple and dumb
  if(comparation_first[0]=='='&&comparation_first[1]=='='){
    result->operator=comparation_eq;
    len=2;
  }else if(comparation_first[0]=='!'&&comparation_first[1]=='='){
    result->operator=comparation_ne;
    len=2;
  }else if(comparation_first[0]=='>'&&comparation_first[1]!='='){
    result->operator=comparation_gt;
    len=1;
  }else if(comparation_first[0]=='>'&&comparation_first[1]=='='){
    result->operator=comparation_ge;
    len=2;
  }else if(comparation_first[0]=='<'&&comparation_first[1]!='='){
    result->operator=comparation_lt;
    len=1;
  }else if(comparation_first[0]=='<'&&comparation_first[1]=='='){
    result->operator=comparation_le;
    len=2;
  }else if(comparation_first[0]=='='&&comparation_first[1]=='~'){
    result->operator=comparation_reg;
    len=2;
  }else{ 
    result->operator=comparation_error;
    fail();
  }

  const char * lhss=start,
    *lhse=comparation_first,
    *rhss=comparation_first+len,
    *rhse=end;
  remove_out_most_bar(&lhss,&lhse);
  remove_out_most_bar(&rhss,&rhse);
  result->oprand[0].start=lhss;result->oprand[0].end=lhse;
  result->oprand[1].start=rhss;result->oprand[1].end=rhse;
  return result;
}

static expression compile_expression(const char * start, const char* end){
  remove_out_most_bar(&start,&end);
  expression result={{NULL},exp_error};

  const char* logic_first=jassonpath_next_punctors_outside_para(start,end,"|&");
  //no binary logic operator found
  if(logic_first==end||!*logic_first){
    //negative
    if(start[0]=='!'){
      result.tag=exp_logic;
      result.logic=NEW(logic_exp);
      result.logic->operator=logic_negtive;
      result.logic->oprand[0]=compile_expression(start+1,end);
    }
    //comparation
    result.tag=exp_compare;
    result.comp=compile_compare(start,end);
    return result;
  }
  //logic operator found
  if(logic_first[0]!=logic_first[1]) fail();//&& or ||
  result.tag=exp_logic;
  result.logic=NEW(logic_exp);
  result.logic->operator = logic_first[0]=='&'?logic_and:logic_or;
  result.logic->oprand[0]=compile_expression(start,logic_first);
  result.logic->oprand[1]=compile_expression(logic_first+2,end);
  return result;
}

static void comparation_free(comparation* exp){
  DELETE(exp);
}

static void expression_free(expression exp);

static void logic_exp_free(logic_exp* exp){
  expression_free(exp->oprand[0]);
  if(exp->operator>logic_negtive)expression_free(exp->oprand[1]);
  DELETE(exp);
}

static void expression_free(expression exp){
  if(exp.tag==exp_logic) logic_exp_free(exp.logic);
  else comparation_free(exp.comp);
}

// for this end should not be NULL
static path_result json_path_get_property(json_t * root, json_t *json, const char *name, const char *end) {
  path_result result={NULL,FALSE};
  if (name[0] == '*') { // select all members
    result.is_collection=TRUE;
    if(name[1]){
      result.result=json_array();
      fail();
    }
    result.result = json_path_get_all_properties(json);
    return result;
  }

  if(name[0]=='('){
    const char* right=jassonpath_next_matched_bracket(name,end,'(',')');
    if(right!=end-1)fail();
    expression exp=compile_expression(name,end);
    
    expression_free(exp);
  }
  
  // number or named property
  const char* seg[4];
  seg[0]=name;
  size_t segn;//number of parameters seprated by ':'
  for(segn=1;segn<4;segn++){
    seg[segn]=jassonpath_next_seprator(seg[segn-1],end,':');
    if(seg[segn]==end)break;
    seg[segn]++;
  }
  seg[segn]=end;
  long seg_int[3];
  int seg_filled[3];//is it a number and been filled?
  {//make a scope to isolate i. workaround for poor supported inline declaration in for
    size_t i;
    for(i=0;i<segn;i++){
      char * end;
      seg_int[i]=strtol(seg[i],&end,10);
      seg_filled[i] = (end!=seg[i]);
      //if(seg_filled[i]&&end!=seg[i+1]) ; error, but we ignore that
    }
  }
  //only one parameter and it's not a number
  if(segn==1&&!seg_filled[0]){ // select named properties
    result.is_collection=FALSE;
    if (!json_is_object(json)) fail();
    const char* pname=jassonpath_strdup_no_terminal(name,end);
    result.result = json_incref(json_object_get(json, pname));
    free((void*)pname);
  }else if(segn==1&&seg_filled[0]){//[i]
    result.is_collection=FALSE;
    if (!json_is_array(json)||!seg_filled[0]) fail();
    result.result = json_incref(json_array_get(json, seg_int[0]));
  }else{//[i:j]([:j] [i:]) or [i:j:k] or [-i:]
    result.is_collection=TRUE;
    result.result=json_array();
    if(!json_is_array(json)) fail();//incorrect input
    long step=1;size_t start=0;size_t end=json_array_size(json);
    int rev=(segn==2&&seg_filled[0]&&!seg_filled[1]&&seg_int[0]<0);
    if(!rev){
      if(seg_filled[0]&&seg_int[0]>=0)start=seg_int[0];
      if(segn==2&&seg_filled[1]&&((size_t)seg_int[1])<end)end=seg_int[1];
      else if(segn==3&&seg_filled[2]&&((size_t)seg_int[2])<end)end=seg_int[2];
      if(segn==3&&seg_filled[1]&&seg_filled[1]!=0)step=seg_int[1];
      //negative step does not mean any thing
      //because we do not arrange output with order
      if(step<0)step=-step;
    }else{
      long start_=end+seg_int[0];//start=end-i
      if(start_>=0)start=start_;
    }
    size_t i;
    for(i=start;i<end;i+=step){
      json_t * ele=json_array_get(json,i);
      json_array_append_new(result.result,ele);
    } 
  }
  return result;
}

static path_result json_path_get_property_col(json_t *root, path_result curr,
                                          const char *name, const char *end
                                          ) {
  path_result result={NULL,FALSE};

  if (curr.is_collection) {
    result.result = json_array();
    size_t index;
    json_t *iter;
    json_array_foreach(curr.result, index, iter) {
      path_result r = json_path_get_property(root, iter, name, end);
      if (r.is_collection)
        json_array_extend(result.result, r.result);
      else
        json_array_append(result.result, r.result);
      json_decref(r.result);
    }
    result.is_collection=TRUE;
  } else {
    result = json_path_get_property(root, curr.result, name, end);
  }
  return result;
}

// on_collection if we are path_get on a collection
// we treat . and [] the same way as property
// and by this we can simplify the implementation a lot
static path_result json_path_get_impl(json_t *root, path_result curr, const char *path,
                                  const char *end) {
  if (!curr.result || path == end ||
      *path == '\0') { // no selection to do. return original collection
    json_incref(curr.result); // note this should looks like a copy to users
    return curr;
  }
  path_result result = {NULL, FALSE};

  int recursive = FALSE;

  int correct_grammar = (path[0] == '.' || path[0] == '[');
  if (!correct_grammar)
    fail();

  recursive = (path[0] == '.' && path[1] == '.'); //.. recursive search of property
  const char *pname = path + (recursive ? 2 : 1);
  const char *pname_end;
  // got the full property name
  if (path[0] == '[') {
    pname_end = jassonpath_next_matched_bracket(path,end,'[',']');
    if (pname_end[0] != ']') fail();
    path = pname_end+1;
  } else {
    pname_end = jassonpath_next_delima(pname, end);
    path = pname_end;
  }
  // if it's string, deal with the string problems
  // todo: unexcape. especially for filters like ["?(@.\"a\")"] kind of things
  if (*pname == '\"') {
    if (pname_end == pname || pname_end[-1] != '\"')
      fail();
    pname++;
    pname_end--;
  }

  if (recursive) {
    //dumb C does not allow to assign struct directly
    path_result new_result = {json_array(),TRUE};
    result=new_result;

    json_incref(curr.result);
    path_result out_layer = curr;

    static const char all[]="*";
    while (out_layer.result &&
           !(json_is_array(out_layer.result) && !json_array_size(out_layer.result))) {
      path_result cur_layer = json_path_get_property_col(root, out_layer, all, all+1);
      json_decref(out_layer.result);
      path_result selected =
          json_path_get_property_col(root,cur_layer, pname, pname_end);
      json_array_extend(result.result, selected.result);
      json_decref(selected.result);
      out_layer = cur_layer;
    }
    json_decref(out_layer.result);
  } else {
    result = json_path_get_property_col(root, curr, pname, pname_end);
  }

  path_result ret = json_path_get_impl(root, result, path, end);
  json_decref(result.result);
  return ret;
}

//json_path_get,
//version with distinction between collection and simple result
path_result json_path_get_distinct(json_t *json, const char *path) {
  path_result result ={NULL,FALSE};
  if (!path || !json) fail();
  if (path[0] == '$')
    path++; // it can be omited safely since current node is the root node
  // note that every json path selection is applyed to a collection of json
  // nodes
  path_result cur={json,FALSE};
  result = json_path_get_impl(json, cur, path, NULL);
  return result;
}

// return NULL if either of arguments is NULL or any error occurs.
// path must be at least zero length and terminated with '\0'
// assuming no extra spaces except for string
// users are reponsible to decref the json_node returned
// extention to standard jsonpath - $."0" $.0 $[0] $["0"] they are all the
// same(for simplify implementation)
json_t* json_path_get(json_t *json, const char *path) {
  return json_path_get_distinct(json,path).result;
}