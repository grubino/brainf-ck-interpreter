#ifndef __PHOENIX_INTRO_H__
#define __PHOENIX_INTRO_H__

//#define BOOST_SPIRIT_DEBUG

#include <iostream>
#include <iterator>
#include <string>
#include <deque>
#include <vector>
#include <stack>

#include <boost/lexical_cast.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/boost_tuple.hpp>

#include <boost/variant/recursive_variant.hpp>
#include <boost/foreach.hpp>

using namespace boost::spirit::qi;
using namespace boost::phoenix;
using namespace std;

// a command string is a sequence of primitive brainf_ck tokens
typedef vector<char> bf_command_string;

/**
 * @struct bf_command_sequence
 * @brief make initializations and seeks constant time.
 * often brainf_ck tokens are repeated many times when a value is initialized or when
 * the turing machine seeks a cell which is somewhat far away from the current cell.
 * bf_command_sequence offers a way to reduce the this from linear time to constant.
 */
struct bf_command_sequence {

  bf_command_sequence() {}
  bf_command_sequence(char c, int i)
  : m_command(c)
  , m_repetitions(i) {}
  bf_command_sequence(const vector<char>& v)
  : m_command(v[0])
  , m_repetitions(v.size()) {}

  char m_command;
  int m_repetitions;

  friend ostream& operator<<(
			     ostream& os,
			     const bf_command_sequence& bfcs
			     );
  
};
ostream& operator<<(ostream& os, const bf_command_sequence& bfcs);


/**
 * @struct bf_clear_cell
 * @brief optimized command type for brainf_ck sequence '[-]'.
 */
struct bf_clear_cell {};
ostream& operator<<(ostream& os, const bf_clear_cell& cc);


/**
 * @struct bf_transfer_cell
 * @brief optimized command type for brainf_ck sequences such as '[->>>+<<<]'.
 */
struct bf_transfer_cell {

  bf_transfer_cell() {}
  bf_transfer_cell(long x, int y)
  : offset(x)
  , quantity(y) {}
  
  long offset;
  int quantity;
  
};
ostream& operator<<(ostream& os, const bf_transfer_cell& tc);


/**
 * @typedef bf_multi_transfer_cell
 * @brief multiple cells can be transferred with the patterns like '[->+>+<<]'
 */
typedef vector<bf_transfer_cell> bf_multi_transfer_cell;
ostream& operator<<(ostream& os, const bf_multi_transfer_cell& tc);


/**
 * @typedef bf_known_command
 * @brief branching type for optimizable commands
 */
typedef boost::variant<
bf_clear_cell
, bf_transfer_cell
  , bf_multi_transfer_cell
  > bf_known_command;


/**
 * @typedef bf_command_variant
 * @brief from the point of view of the @turing_machine_visitor, these are atomic.
 */
typedef boost::variant<
bf_known_command
, bf_command_sequence
  > bf_command_variant;


/**
 * @struct bf_statement
 * @brief statements consist of expressions enclosed in [].
 */
struct bf_statement;

/**
 * @typedef bf_expression
 * @brief the main non-terminal type
 */
typedef boost::variant<
boost::recursive_wrapper<bf_statement>
, std::vector<bf_command_variant>
  > bf_expression;



struct bf_statement {
  std::vector<bf_expression> stmt;
};

BOOST_FUSION_ADAPT_STRUCT(
			  bf_statement,
			  (std::vector<bf_expression>, stmt)
			  )

/**
 * @struct turing_machine
 * @brief interprets the stream of commands from the turing_machine_visitor.
 */
struct turing_machine {

  turing_machine()
  : m_data(30000, 0),
    m_data_ptr(0) {}

  void process_command_sequence(const bf_command_sequence& c);
  void process_command(const bf_clear_cell&) {
    m_data[m_data_ptr] = 0;
  }
  void process_command(const bf_transfer_cell& tc) {
    char temp = m_data[m_data_ptr];
    move_data_ptr(tc.offset);
    alter_data(tc.quantity * temp);
    move_data_ptr(-tc.offset);
  }

  bool is_zero() const { return (m_data[m_data_ptr] == 0); }
  void alter_data(char a);
  void move_data_ptr(long i);
  void output_data() const;
  void input_data();

  void print_state() {
    cout << "data pointer position: " << m_data_ptr << endl;
    cout << "data pointer content: " << static_cast<int>(m_data[m_data_ptr]) << endl;
  }

  std::deque<char> m_data;
  long m_data_ptr;

};

/**
 * @struct turing_machine_visitor
 * @brief visitor dispatching parsed commands.
 * ======================
 * this structure acts as a dispatch for different command types from the
 * brainf*ck input.
 */

struct turing_machine_visitor : boost::static_visitor<> {

  turing_machine_visitor(turing_machine& tm)
    : m_tm(tm) {}

  void operator()(const bf_clear_cell& cc) const {
    m_tm.process_command(cc);
  }

  void operator()(const bf_multi_transfer_cell& mtc) const {
    BOOST_FOREACH(bf_transfer_cell tc, mtc) {
      m_tm.process_command(tc);
    }
    m_tm.process_command(bf_clear_cell());
  }
  
  void operator()(const bf_transfer_cell& tc) const {
    m_tm.process_command(tc);
    m_tm.process_command(bf_clear_cell());
  }
  
  void operator()(bf_command_sequence& cs) const {
    m_tm.process_command_sequence(cs);
  }
  
  void operator()(const bf_known_command& kc) const {
    boost::apply_visitor(*this, kc);
  }
  
  void operator()(const std::vector<bf_command_variant>& v) const {
    BOOST_FOREACH(bf_command_variant cv, v) {
      boost::apply_visitor(*this, cv);
    }
  }

  void operator()(const bf_statement& s) const {
    while(!m_tm.is_zero()) {
      BOOST_FOREACH(bf_expression bf_e, s.stmt) {
	boost::apply_visitor(*this, bf_e);
      }
    }
  }
  
  turing_machine& m_tm;
  
};

/**
 * @struct bf_grammar
 * @brief parse the raw brainf_ck input.
 * ==========
 * the grammar parses the input into optimized structures so that we don't have to deal
 * directly with the brainf*ck input.
 */
struct bf_grammar
  : grammar<bf_command_string::iterator, vector<bf_expression>()> {

  bf_grammar()
    : bf_grammar::base_type(start) {

    start = +expression;
    expression %= command_sequence | statement;
    statement %= lit('[') >> +expression >> lit(']');
    command_sequence %= +command_group;
    command_group %= known_command_sequence | command_token;

    known_command_sequence %=
      clear_cell
      | transfer_cell
      | multi_transfer_cell
      ;
    command_token =
      (
       +increment
       | +decrement
       | +move_right
       | +move_left
       | +put_output
       | +get_input
       )
      [_val = construct<bf_command_sequence>(_1)]
      ;
    
    clear_cell = boost::spirit::qi::string("[-]")[_val = construct<bf_clear_cell>()]
      ;
    transfer_cell %=
      transfer_cell_left
      | transfer_cell_right
      ;
    
    transfer_cell_right =
      eps[_a = val(0), _b = val(0)] >>
      open_bracket >>
      decrement >>
      +(move_right[_a += val(1)]) >>
      +(
	increment[_b += val(1)]
	| decrement[_b -= val(1)]
	) >>
      repeat(_a)[move_left] >>
	close_bracket[_val = construct<bf_transfer_cell>(_a, _b)]
      ;
    transfer_cell_left =
      eps[_a = val(0), _b = val(0)] >>
      open_bracket >>
      decrement >>
      +(move_left[_a += val(1)]) >>
      +(
	increment[_b += val(1)]
	| decrement[_b -= val(1)]
	) >>
      repeat(_a)[move_right] >>
	close_bracket[_val = construct<bf_transfer_cell>(-_a, _b)]
      ;

    multi_transfer_cell =
      eps[_a = val(0)] >>
      open_bracket >>
      decrement >>
      multi_transfer_cell_body
      [
       _a = at_c<0>(_1)
       , _val = at_c<1>(_1)
       ] >>
      multi_transfer_cell_terminate(_a)
      ;
    
    multi_transfer_cell_body =
      eps[_a = val(0)] >>
      +(
	multi_transfer_phrase(_a)[
				  _a = at_c<0>(_1)
				  , push_back(
					      at_c<1>(_val)
					      , at_c<1>(_1)
					      )
				  ]
	)[at_c<0>(_val) = _a]
      ;
    multi_transfer_cell_terminate =
      multi_transfer_cell_terminate_right(_r1)
      | multi_transfer_cell_terminate_left(_r1)
      ;
    multi_transfer_cell_terminate_right =
      repeat(-_r1)[move_right] >>
      close_bracket
      ;
    multi_transfer_cell_terminate_left =
      repeat(_r1)[move_left] >>
      close_bracket
      ;

    multi_transfer_phrase =
      eps[_a = _r1, _b = val(0)] >>
      multi_transfer_seek_phrase[_a += _1] >>
      multi_transfer_modify_phrase[
				   _b += _1
				   , at_c<0>(_val) = _a
				   , at_c<1>(_val) = construct<bf_transfer_cell>(_a, _b)
				   ]
      ;

    multi_transfer_seek_phrase =
      +(
	move_right[_val += val(1)]
	| move_left[_val -= val(1)]
	)
      ;
    multi_transfer_modify_phrase =
      +(
	increment[_val += val(1)]
	| decrement[_val -= val(1)]
	)
      ;

    increment %= char_('+');
    decrement %= char_('-');
    move_right %= char_('>');
    move_left %= char_('<');
    open_bracket %= char_('[');
    close_bracket %= char_(']');
    put_output %= char_('.');
    get_input %= char_(',');

    
    BOOST_SPIRIT_DEBUG_NODE(expression);
    BOOST_SPIRIT_DEBUG_NODE(statement);
    BOOST_SPIRIT_DEBUG_NODE(command_sequence);
    BOOST_SPIRIT_DEBUG_NODE(command_group);
    BOOST_SPIRIT_DEBUG_NODE(command_token);
    BOOST_SPIRIT_DEBUG_NODE(clear_cell);
    BOOST_SPIRIT_DEBUG_NODE(transfer_cell);
    BOOST_SPIRIT_DEBUG_NODE(transfer_cell_left);
    BOOST_SPIRIT_DEBUG_NODE(transfer_cell_right);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_cell);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_cell_body);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_cell_terminate);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_cell_terminate_right);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_cell_terminate_left);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_phrase);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_seek_phrase);
    BOOST_SPIRIT_DEBUG_NODE(multi_transfer_modify_phrase);
    BOOST_SPIRIT_DEBUG_NODE(increment);
    BOOST_SPIRIT_DEBUG_NODE(decrement);
    BOOST_SPIRIT_DEBUG_NODE(move_decide);
    BOOST_SPIRIT_DEBUG_NODE(move_right);
    BOOST_SPIRIT_DEBUG_NODE(move_left);
    BOOST_SPIRIT_DEBUG_NODE(get_input);
    BOOST_SPIRIT_DEBUG_NODE(put_output);

  }
  
  rule<bf_command_string::iterator, vector<bf_expression>()> start;

  rule<bf_command_string::iterator, bf_expression()> expression;
  rule<bf_command_string::iterator, bf_statement()> statement;

  rule<bf_command_string::iterator, vector<bf_command_variant>()> command_sequence;

  rule<bf_command_string::iterator, bf_command_variant()> command_group;
  rule<bf_command_string::iterator, bf_command_sequence()> command_token;
  rule<bf_command_string::iterator, bf_known_command()> known_command_sequence;

  rule<bf_command_string::iterator, bf_clear_cell()> clear_cell;
  rule<bf_command_string::iterator, bf_transfer_cell()> transfer_cell;
  rule<bf_command_string::iterator, bf_transfer_cell(), locals<long, int> > transfer_cell_left;
  rule<bf_command_string::iterator, bf_transfer_cell(), locals<long, int> > transfer_cell_right;
  
  rule<bf_command_string::iterator, bf_multi_transfer_cell(), locals<long> > multi_transfer_cell;

  rule<
    bf_command_string::iterator,
    boost::tuple<long, bf_multi_transfer_cell>(),
    locals<long>
    > multi_transfer_cell_body;

  rule<
    bf_command_string::iterator,
    void(long)
    > multi_transfer_cell_terminate;
  rule<
    bf_command_string::iterator,
    void(long)
    > multi_transfer_cell_terminate_right;
  rule<
    bf_command_string::iterator,
    void(long)
    > multi_transfer_cell_terminate_left;

  rule<
    bf_command_string::iterator,
    boost::tuple<long, bf_transfer_cell>(long),
    locals<long, int>
    > multi_transfer_phrase;
  
  rule<bf_command_string::iterator, long(), locals<long> > multi_transfer_seek_phrase;
  rule<bf_command_string::iterator, int(), locals<int> > multi_transfer_modify_phrase;
  
  rule<bf_command_string::iterator, char()> increment;
  rule<bf_command_string::iterator, char()> decrement;
  rule<bf_command_string::iterator, char()> move_left;
  rule<bf_command_string::iterator, char()> move_right;
  rule<bf_command_string::iterator, char()> move_decide;
  rule<bf_command_string::iterator, char()> open_bracket;
  rule<bf_command_string::iterator, char()> close_bracket;
  rule<bf_command_string::iterator, char()> get_input;
  rule<bf_command_string::iterator, char()> put_output;

};


#endif
