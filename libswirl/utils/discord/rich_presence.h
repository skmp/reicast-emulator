#include "deps/discord_game_sdk/cpp/discord.h"
#include <memory>
#include <string>
#include <thread>
#include <chrono>

namespace discord {

	class RichPresence {
	public:

		~RichPresence();

		RichPresence(ClientId clientId) {
			this->clientId = clientId;

			this->startTimestamp = time(0);

			discord::Core::Create(this->clientId, DiscordCreateFlags_Default, &this->core);
			this->state.core.reset(this->core);

			if (!state.core) {
				// TODO: return error
				//std::cout << "Failed to instantiate Discord!";
			}
		}

		void UpdateInformation() {
			this->activity.SetDetails(this->details.c_str());
			this->activity.GetAssets().SetSmallImage(this->smallImage.c_str());
			this->activity.GetAssets().SetSmallText(this->smallText.c_str());
			this->activity.GetTimestamps().SetStart(this->startTimestamp);
			this->activity.SetType(this->type);
			this->activity.GetAssets().SetLargeImage(this->largeImage.c_str());
			this->activity.GetAssets().SetLargeText(this->largeText.c_str());
			this->state.core->ActivityManager().UpdateActivity(this->activity, [](discord::Result result) {
				//(result == discord::Result::Ok)
				}
			);
		}

		void Tick() {
			this->state.core->RunCallbacks();
		}

		void SetDetails(std::string details) {
			this->details = details;
		}

		void SetSmallImage(std::string assetName) {
			this->smallImage = assetName;
		}

		void SetSmallText(std::string text) {
			this->smallText = text;
		}

		void SetLargeImage(std::string assetName) {
			this->largeImage = assetName;
		}

		void SetLargeText(std::string text) {
			this->largeText = text;
		}

		void SetStart(Timestamp timestamp) {
			this->startTimestamp = timestamp;
		}

		void SetType(ActivityType type) {
			this->type = type;
		}

	private:

		ActivityType type = ActivityType(0);

		struct DiscordState {
			std::unique_ptr<discord::Core> core;
		};

		DiscordState state{};
		discord::Core* core{};

		discord::Activity activity{};

		ClientId clientId;

		std::string details = "";
		std::string smallImage = "";
		std::string smallText = "";
		std::string largeImage = "";
		std::string largeText = "";

		Timestamp startTimestamp;

	};
	
	struct DiscordTickParams {
		RichPresence* richPresence;
		bool* discordLook;
	};

	void* discordTickFunc(void* params) {
		DiscordTickParams* discordTickParams = (struct DiscordTickParams*)params;
		do {
			discordTickParams->richPresence->Tick();
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		} while (*(discordTickParams->discordLook));
		return nullptr;
	}

}
