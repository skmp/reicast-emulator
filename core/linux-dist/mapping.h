#include <map>
#include "linux-dist/main.h"

struct ControllerAxis
{
	DreamcastController id;
	int code;
	bool inverted;
};

class ControllerMapping
{
	private:
		std::map<int, DreamcastController> buttons;
		std::map<int, ControllerAxis> axis_codes;
		std::map<DreamcastController, ControllerAxis*> axis_ids;

	public:
		const char* name;
		
		void set_button(int code, DreamcastController button_id);
		DreamcastController get_button(int code);

		void set_axis(int code, ControllerAxis axis);
		ControllerAxis* get_axis_by_code(int code);
		ControllerAxis* get_axis_by_id(DreamcastController axis_id);

		void load(FILE* file);
};