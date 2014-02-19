
#include"brainf_ck.h"

#include<boost/iterator/filter_iterator.hpp>
#include<boost/assign.hpp>

#include<iostream>
#include<fstream>
#include<iomanip>
#include<iterator>

#include<set>

struct bf_filter_predicate {

  bf_filter_predicate(const std::set<char>& allowed_chars) 
    : m_allowed(allowed_chars) {}

  bool operator()(char c) const {
    return m_allowed.count(c) > 0;
  }

private:
  const std::set<char>& m_allowed;

};

using namespace std;
using namespace boost::spirit::qi;

int main(int argc, char** argv) {

  if(argc != 2)
    return 1;

  std::set<char> allowed_chars = 
    boost::assign::list_of('+')('-')('<')('>')('[')(']')('.');
  
  ifstream program_file(argv[1]);
  program_file >> skipws;

  std::vector<char> program;

  std::copy(boost::make_filter_iterator(bf_filter_predicate(allowed_chars)
					, istream_iterator<char>(program_file)
					, istream_iterator<char>())
	    , boost::make_filter_iterator(bf_filter_predicate(allowed_chars)
					  , istream_iterator<char>()
					  , istream_iterator<char>())
	    , back_inserter(program));

  bf_grammar bfg;
  std::vector<char>::iterator it_(program.begin());
  std::vector<bf_expression> ast;
  
  if(parse(it_, program.end(), bfg, ast)) {
    
    turing_machine tm;
    BOOST_FOREACH(bf_expression bf_e, ast) {
      boost::apply_visitor(turing_machine_visitor(tm), bf_e);
    }
    
  } else {
    std::cout << "there was a problem parsing your program." << std::endl;
  }

  
  return 0;
  
}
