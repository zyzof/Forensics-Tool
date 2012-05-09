#include <string>

void *listener(void *p);

void get_input(Case current_case, char *input);
std::string get_input_string(Case current_case);
void put_output(Case current_case, const char* output);
void put_output_string(Case current_case, std::string output);
void put_output_and_log(Case current_case, const char* output, bool timestamp);
