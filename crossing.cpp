#include <set>
#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <assert.h>
#include <yaml-cpp/yaml.h>

//Author: Edward Horn
//Date Created: 5/20/2019
//To compile:   g++ -std=c++17 crossing.cpp -lyaml-cpp

//The task is to get people from one side of a bridge to the other at night with only one torch.
//People travel at different speeds.  When travelling together they do so at the speed of the slowest.
//The bridge can hold no more than 2 people at a time.  
//To cross the bridge at least one of them must be holding the torch.  
//Our goal is to find an order of crossing that will take minimal time, and print the results.

//For our model we will create a left and right bank of the river and a bridge.
//People will begin on the left bank.
//The entities in the model will include: 
//	Person:  description of one of the people.
//		When we read in a person's description we are given a name and a speed.
//		But two people can have the same name, so we will create a unique ID for each as we read.
//  Area: Area people can be moved to.
//  Crossing: Left Bank, Bridge and Right Bank Areas.
//  CrossingHistory:  the complete history of how we got across the bridge
//  FastCrossing: our strategy for quickly crossing the bridge
//
//  Constraints:  No more than 2 people on bridge at a time.
//	
//  For our purposes there is no reason to model the torch. 
//  We could get away with not modelling the bridge, simply keep a running count of the elapsed time.
//    But by including the bridge in our model we have can create complete snapshots of our state
//    and a complete record of crossings. A complete history like this is clearer to work with
//    and much more flexible for future development and debugging.
//
//  By splitting out or strategy into its own object we open to door to reuse other strategies in
//     our tests.

// A Person.  Simple data holder
struct Person {
	std::string name;  //The name of the person.  
				//TODO: optimize by creating master table indexed by ID and dropping from struct.
	int ID;  	//unique ID.  To be created on the fly as data is read in.
				//this will allow multiple people with same name.
	double time;  	//time taken to cross
};

//comparators
bool operator == (const Person& lhs, const Person& rhs) {return lhs.ID == rhs.ID; }
bool operator < (const Person& lhs, const Person& rhs) { 
	if (lhs.time == rhs.time) {
		return lhs.ID < rhs.ID;  //one reason the unique ID is handy
	}
	return lhs.time < rhs.time; //slower people are lesser people
}

// An area people can be moved to/from.
// All data is local, so no need for explicit assignment/destructors/etc
class Area {
	std::set<Person, std::less<Person> > _people;
  public:
	Area(const std::set<Person> people) : _people(people) {}
	Area(const std::vector<Person> people) {
		for(auto p: people) { _people.insert(p); }
		}
	Area() {}
	
	void add_person(const Person p) {
		_people.insert(p);
	}
	
	bool empty() const { return (0 == _people.size()); } //no one here
	size_t size() const { return _people.size(); }  //used for asserts.  2 people allowed on bridge
	
	
	const Person fastest() const {  //the fastest in area
		assert(!empty());  //must have people to move
		return (*_people.begin()); 
		}
	const Person slowest() const {  
		assert(!empty());
		return (*_people.rbegin()); 
		}
	
	const std::set<Person> people() { return _people; }
	
	// move person to another Area
	void transfer(const Person p, Area& to) {
		_people.erase(p);
		to._people.insert(p);
	}
	
	// move all people to another Area
	void transfer_all(Area& to) {
		while(!empty()) {
			transfer(slowest(), to);
		}
	}
		
	friend bool operator == (const Area& lhs, const Area &rhs);
	
	// print out.  yes we could use an << operator but this is nice for debugging.
	void dump() {
		for(auto p: _people) { std::cout << p.name << " "; } 
		std::cout << std::endl;
		}
};

bool operator == (const Area& lhs, const Area &rhs) { return lhs._people == rhs._people; }

// The current state of a crossing.  Who is where.  We keep info for bookeeping.
// All data is local.  Will use automatic assgnment/destructor/etc
// Another reason to use local, non-reference data is to allow us to create an exhaustive recursive
//   version that we are sure will create an optimized result for use in testing our fast version.
class CrossingState {
	Area _lb;  	//left bank, where we start
	Area _bridge;	//The bridge.  
	Area _rb;	//right bank, where we end
	
  public:
	CrossingState (const std::vector<Person> people) : _lb(people) {}
	CrossingState () {} //empty initializer
	CrossingState (const std::string filename);  //load from a yaml file
	
	Area& left() { return _lb; }
	Area& right() { return _rb; }
	Area& bridge() { return _bridge; }
	
	//methods for moving people across the bridge.  
	//although this seems wordy, it will cut down the possibilites of mistakes later in the code
	
	void l_to_b(const Person& p) { 
		assert(_bridge.size() < 2);  //max of two people allowed on bridge
		_lb.transfer(p, _bridge); 
		} //move person from left bank to bridge
		
	void b_to_r(const Person& p) { //move person from bridge to right bank
		_bridge.transfer(p, _rb); 
		}
		
	void r_to_b(const Person& p) { //right bank to bridge
		assert(_bridge.size() < 2);
		_rb.transfer(p, _bridge); 
		}
		
	void b_to_l(const Person& p) { //bridge to left bank
		_bridge.transfer(p, _lb); 
		}
		
	void all_b_to_r() { // move everyone from bridge to right bank
		_bridge.transfer_all(_rb); 
		} 
	void all_b_to_l() { //move everyone on bridge to left
		_bridge.transfer_all(_lb); 
		} 
	
	double speed_across_bridge() const { //How fast does it take to cross the bridge?
		if(_bridge.empty()) return 0;
		else return _bridge.slowest().time; //slowest person
	}
	
	void dump() {
		std::cout << "LEFT: ";
		left().dump();
		std::cout << "BRIDGE: ";
		bridge().dump();
		std::cout << "RIGHT: ";
		right().dump();
		std::cout << std::endl;
	}
		 
	friend bool operator == (const CrossingState& lhs, const CrossingState& rhs);
};

bool operator == (const CrossingState& lhs, const CrossingState& rhs) {
	return (lhs._lb == rhs._lb && lhs._bridge == rhs._bridge && lhs._rb == rhs._rb);
}

//load from yaml
CrossingState::CrossingState(const std::string filename) {
	int ID = 0;
	YAML::Node base = YAML::LoadFile(filename);
	if (base.IsNull()) { 
		std::cout << "NO FILE" << filename << std::endl; 
		return; //probably legal
		}
		
	YAML::Node people = base["people"];
	if (people.IsNull()) { 
		std::cout << "NO PEOPLE IN FILE " << filename << std::endl; 
		return; // legal?? not sure.  otherwise an assert or throw
		}
	for(auto i: people) {
		Person p = { i["name"].as<std::string>(), ID, i["time"].as<double>() };
		left().add_person(p);
		++ID; //our unique ID in case of multiples.  A safety measure.
	}
}

// The history of our crossings.  We will build this in the course of our algorithhm
struct CrossingHistory {
  private:
	std::list<CrossingState> _history;
	
  public:
  
	// add the current state to the history
	void record(const CrossingState& state) { _history.push_back(state); }
	
	// the total crossing time for all states in history
	double total_time() {
		double t = 0;
		for (auto c: _history) {
			t += c.speed_across_bridge();
		}
		return t;
	}
	
	// is a state already in the list?
	// the fast version of the algorithm does not need this
	// it is for exhaustive versions that need to avoid cycles
	bool visited(const CrossingState& state) const {
		return (0 != std::count(_history.begin(), _history.end(), state));
	}
	
	void dump() { 
		for(auto h: _history)
			h.dump();
		std::cout << "TOTAL TIME " << total_time() << std::endl; 
		}
};
	
//Our fast version of the algorithm.  A recursive exhaustive version would be quite different.
class FastCrossing {
	CrossingHistory _hist;
	CrossingState _state;
	
	void snap() { _hist.record(_state); } //add a snapshot of the current state to history
		
	// this may seem redundant, but being explicit minimizes chances of screwing up left/right etc
	Area& left() { return _state.left(); }
	Area& bridge() { return _state.bridge(); }
	Area& right() { return _state.right(); }
	void fastest_l_to_b() { _state.l_to_b(left().fastest());} //move fastest from left bank to bridge
	void fastest_r_to_b() { _state.r_to_b(right().fastest()); } //fastest right to bridge
	void slowest_l_to_b() { _state.l_to_b(_state.left().slowest()); }
	void all_b_to_l() { _state.all_b_to_l(); }
	void all_b_to_r() { _state.all_b_to_r(); }
	
	bool left_empty() { return _state.left().empty(); }  //the left bank is empty.  done
	
	void retrieve_fastest() { //send the fastest from right to left
		fastest_r_to_b();
		snap();
		all_b_to_l();
		snap();
	}
	
	void send_slowest() { //send the two slowest from left to right
		slowest_l_to_b();
		slowest_l_to_b();
		snap();
		all_b_to_r();
		snap();
	}

	void clear() {
		_hist = CrossingHistory();  //clear history
	}
	
  public:
	
	//Cross the bridge!
	CrossingHistory cross(const CrossingState& initial_state) {
		clear();
		
		_state = initial_state;
		snap();
		
		//special cases, one or two people:
		if(0 == left().size()) return _hist;  //nothing
		if(1 == left().size()) {
			fastest_l_to_b();
			snap();
			all_b_to_r();
			snap();
			return _hist;
		}
		

		while(!left_empty()) {
			//send the two fastest as couriers for future crossings
			fastest_l_to_b();
			fastest_l_to_b();
			snap();
			all_b_to_r();  //the fast couriers are not both on the other side
			snap();
			//at this point we are set up to bring biggest two over
			if (left_empty()) break;  //no one left. we are done
			retrieve_fastest();  //send the courier back with torch as future guide
								//we have left the second fastest on the other side
			send_slowest(); //send over the two slowest guys
			if (!left_empty()) {
				retrieve_fastest(); //bring back the other fast guy with the torch
			}
		}
		return _hist;
	}	
};
	
// NOTE:  I am fairly confident this will create an optimized result.
// If I really wanted to make sure, I would write a brute-force exhaustive recursive version
// that would create all possible paths.  We would use the shortest result to test against a variety
// of inputs.  I designed the basic classes with this in mind. For example, all member data is by
// value, making copying less complicated for use in a recursion.	

int main(int argc, char **argv) {
	
	FastCrossing crossing;	

	CrossingState state (  //default test
		{
		{"A", 1, 1},
		{"B", 2, 2},
		{"C", 3, 5},
		{"D", 4, 10}
		} );
		
	if(argc > 1) {
		state = CrossingState(argv[1]);
	}
	
	CrossingHistory hist = crossing.cross(state);
	hist.dump();
			
	return 0;
}