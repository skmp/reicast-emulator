/* SimpleBimap
 *
 * A basic implementation of a bidirectional map that not only allows
 * you to get a mapped value from a key, but also the other way around.
 * Deleting elements and other fancy (and not-so-fancy) stuff is not
 * supported.
 *
 * Usage example:
 * 	SimpleBimap<std::string, std::string> bimap;
 *  bimap.insert("foo", "bar");
 *  printf("foo -> %s\n", bimap.get_by_key("foo")->c_str());
 *  printf("bar <- %s\n", bimap.get_by_value("bar")->c_str());
 *  if(bimap.get_by_key("somekey") == NULL)
 *    puts("somekey not found");
 *
 * The above example's output:
 *  foo -> bar
 *  bar <- foo
 *  somekey not found
 */
#include <cstddef>
#include <utility>
#include <map>

template<typename T1, typename T2>
class SimpleBimap
{
	private:
		typedef typename std::map<T2, T1*> MapA;
		typedef typename std::map<T1, T2*> MapB;
		MapA map_a;
		MapB map_b;
	public:
		void insert(const T1& a, const T2& b)
		{
			// create first pair
			typename MapA::iterator iter_a = map_a.insert(std::pair<T2, T1*>(b, NULL)).first;
			T2* ptr_b = const_cast<T2*>(&(iter_a->first));

			// insert second pair (a, pointer_to_b)
			typename MapB::iterator iter_b = map_b.insert(std::pair<T1, T2*>(a, ptr_b)).first; 

			// update pointer in map_a to point to a
			T1* ptr_a = const_cast<T1*>(&(iter_b->first));
			iter_a->second = ptr_a;
		}

		const T2* get_by_key(const T1 &a)
		{
			typename MapB::iterator it = this->map_b.find(a);
			if(it != this->map_b.end())
			{
				return (it->second);
			}
			return NULL;
		}

		const T1* get_by_value(const T2 &b)
		{
			typename MapA::iterator it = this->map_a.find(b);
			if(it != this->map_a.end())
			{
				return (it->second);
			}
			return NULL;
		}
};