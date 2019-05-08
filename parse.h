#ifndef JANSSONPATH_PARSE
#define JANSSONPATH_PARSE
//functions for parsing
const char* jassonpath_match_string(const char* in,const char* end);
const char *jassonpath_next_delima(const char *in, const char *end) ;
const char* jassonpath_next_matched_bracket(const char* in,const char* end, char left, char right);
const char *jassonpath_next_seprator(const char *in, const char *end, char sep);
const char *jassonpath_strdup_no_terminal(const char *start, const char *end);
const char *jassonpath_next_punctor_outside_para(const char *in, const char *end, char sep);
const char *jassonpath_next_punctors_outside_para(const char *in, const char *end, char* sep);
#endif
