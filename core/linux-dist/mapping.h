#include <map>
#include "linux-dist/main.h"
#include "linux-dist/bimap.h"

typedef DreamcastController InputButtonID;
typedef DreamcastController InputAxisID;
typedef int InputButtonCode;
typedef int InputAxisCode;

class InputMapping
{
	private:
		SimpleBimap<InputButtonID, InputButtonCode> buttons;
		SimpleBimap<InputAxisID, InputAxisCode> axes;
		std::map<InputAxisCode, bool> axes_inverted;

	public:
		const char* name;
		
		void set_button(InputButtonID id, InputButtonCode code);
		const InputButtonID* get_button_id(InputButtonCode code);
		const InputButtonCode* get_button_code(InputButtonID id);

		void set_axis(InputAxisID id, InputAxisCode code, bool inverted);
		const InputAxisID* get_axis_id(InputAxisCode code);
		const InputAxisCode* get_axis_code(InputAxisID id);
		bool get_axis_inverted(InputAxisCode code);

		void load(FILE* file);
		void print();
};
