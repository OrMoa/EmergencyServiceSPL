#include "../include/event.h"
#include "../include/json.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <cstring>

#include "../include/keyboardInput.h"

using namespace std;
using json = nlohmann::json;

Event::Event(std::string channel_name, std::string city, std::string name, int date_time,
             std::string description, std::map<std::string, std::string> general_information)
    : channel_name(channel_name), city(city), name(name),
      date_time(date_time), description(description), general_information(general_information), eventOwnerUser("")
{
}

Event::Event(const std::string &frame_body): channel_name(""), city(""), 
                                             name(""), date_time(0), description(""), general_information(),
                                             eventOwnerUser("")
{
    std::cout << "[DEBUG] Starting to parse frame body" << std::endl;
    stringstream ss(frame_body);
    string line;
    string eventDescription;
    map<string, string> general_information_from_string;
    bool inGeneralInformation = false;
    while(getline(ss,line,'\n')){
        vector<string> lineArgs;
        if(line.find(':') != string::npos) {
            split_str(line, ':', lineArgs);
            string key = lineArgs.at(0);
            string val;
            if(lineArgs.size() == 2) {
                val = lineArgs.at(1);
                std::cout << "[DEBUG] Parsed key: '" << key << "', value: '" << val << "'" << std::endl;
            }
            if(key == "user") {
                eventOwnerUser = trim(val);
                std::cout << "[DEBUG] Set eventOwnerUser: " << val << std::endl;
            }
            if(key == "channel name") {
                channel_name = trim(val);
                std::cout << "[DEBUG] Set channel_name: " << val << std::endl;
            }
            if(key == "city") {
                city = trim(val);
                std::cout << "[DEBUG] Set city: " << val << std::endl;
            }
            else if(key == "event name") {
                name = trim(val);
                std::cout << "[DEBUG] Set event name: " << val << std::endl;
            }
            else if(key == "date time") {
                date_time = std::stoi(val);
                std::cout << "[DEBUG] Set date_time: " << date_time << std::endl;
            }
            else if(key == "general information") {
                inGeneralInformation = true;
                std::cout << "[DEBUG] Entering general information section" << std::endl;
                continue;
            }
            else if(key == "description") {
                std::cout << "[DEBUG] Reading description" << std::endl;
                while(getline(ss,line,'\n')) {
                    eventDescription += line + "\n";
                }
                description = eventDescription;
                std::cout << "[DEBUG] Set description: " << description << std::endl;
            }

            if(inGeneralInformation) {
                string trimmedKey = trim(key.substr(1));
                string trimmedVal = trim(val);
                std::cout << "[DEBUG] Added general info - Key: '" << key.substr(1) 
                    << "', Value: '" << val << "'" << std::endl;
            }
        }
    }
    general_information = general_information_from_string;
    std::cout << "[DEBUG] Finished parsing frame body" << std::endl;
}

Event::~Event()
{
}

void Event::setEventOwnerUser(std::string setEventOwnerUser) {
    eventOwnerUser = setEventOwnerUser;
}

const std::string &Event::getEventOwnerUser() const {
    return eventOwnerUser;
}

const std::string &Event::get_channel_name() const
{
    return this->channel_name;
}

const std::string &Event::get_city() const
{
    return this->city;
}

const std::string &Event::get_name() const
{
    return this->name;
}

int Event::get_date_time() const
{
    return this->date_time;
}

const std::map<std::string, std::string> &Event::get_general_information() const
{
    return this->general_information;
}

const std::string &Event::get_description() const
{
    return this->description;
}

void Event::split_str(const std::string& str, char delimiter, std::vector<std::string>& out) {
    out.clear(); // Ensure the vector is empty before filling it
    std::string token;
    std::istringstream ss(str);

    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) { // Skip empty tokens
            out.push_back(token);
        }
    }

    // Handle case where str ends with delimiter
    if (!str.empty() && str.back() == delimiter) {
        out.push_back("");
    }
}

names_and_events parseEventsFile(std::string json_path)
{
    std::ifstream f(json_path);
    json data = json::parse(f);

    std::string channel_name = data["channel_name"];

    // run over all the events and convert them to Event objects
    std::vector<Event> events;
    for (auto &event : data["events"])
    {
        std::string name = event["event_name"];
        std::string city = event["city"];
        int date_time = event["date_time"];
        std::string description = event["description"];
        std::map<std::string, std::string> general_information;
        for (auto &update : event["general_information"].items())
        {
            if (update.value().is_string())
                general_information[update.key()] = update.value();
            else
                general_information[update.key()] = update.value().dump();
        }

        events.push_back(Event(channel_name, city, name, date_time, description, general_information));
    }
    names_and_events events_and_names{channel_name, events};

    return events_and_names;
}

std::string Event::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}