/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Author : Richard GAYRAUD - 04 Nov 2003
 *           From Hewlett Packard Company.
 *           Charles P. Wright from IBM Research
 */
#include <map>
#include <iterator>
#include <list>
#include <sys/types.h>
#include <string.h>
#include <assert.h>

#include "sipp.hpp"

listener_map listeners;

unsigned int next_number = 1;
const int SM_UNUSED = -1;

listener *get_listener(const char *id)
{
	listener_map::iterator listener_it = listeners.find(listener_map::key_type(id));
	if (listener_it == listeners.end()) {
		return nullptr;
	}
	return listener_it->second;
}

void add_listener(const char *id, listener *obj)
{
	listeners.insert(std::pair<listener_map::key_type,listener *>(listener_map::key_type(id), obj));
}

void remove_listener(const char *id)
{
	listener_map::iterator listener_it;
	listener_it = listeners.find(listener_map::key_type(id));
	listeners.erase(listener_it);
}

listener::listener(const char *id, bool listening, int userId, bool isAutomatic)
{
	/* For automatic answer calls to an out of call request, we must not */
	/* increment the input files line numbers to not disturb */
	/* the input files read mechanism (otherwise some lines risk */
	/* to be systematically skipped */
	if (!isAutomatic) {
		m_lineNumber = new file_line_map();
		for (file_map::iterator file_it = inFiles.begin();
			 file_it != inFiles.end();
			 file_it++) {
			(*m_lineNumber)[file_it->first] = file_it->second->nextLine(userId);
		}
	} else {
		m_lineNumber = nullptr;
	}
	this->userId = userId;

	if (id == nullptr) {
		this->id = strdup(build_call_id());
	}
	else {
		this->id = strdup(id);
	}
    this->listening = false;
    if (listening) {
        startListening();
    }
}

void listener::startListening()
{
    assert(!listening);
    add_listener(id, this);
    listening = true;
}

void listener::stopListening()
{
    assert(listening);
	remove_listener(id);
    listening = false;
}

char *listener::getId()
{
    return id;
}

listener::~listener()
{
    if (listening) {
        stopListening();
    }
    free(id);
    id = nullptr;
}

char *listener::build_call_id()
{
	static char call_id[MAX_HEADER_LEN];
	char field_buff[MAX_HEADER_LEN] = {'\0'};
	char field_name[11] = {'\0'}; // INT_MAX as string + 1 for null byte
	const char *src = call_id_string;
	char *field_chk;
	char *field_ptr = field_buff;
	int tmp_len, field_num;
	int count = 0;

	if (!next_number) {
		next_number++;
	}

	while (*src && count < MAX_HEADER_LEN-1) {
		if (*src == '%') {
			++src;
			switch(*src++) {
				case 'u':
					count += snprintf(&call_id[count], MAX_HEADER_LEN-count-1, "%u", next_number);
					break;
				case 'p':
					count += snprintf(&call_id[count], MAX_HEADER_LEN-count-1, "%u", pid);
					break;
				case 's':
					count += snprintf(&call_id[count], MAX_HEADER_LEN-count-1, "%s", local_ip);
					break;
				case 'f':
					tmp_len = (int) strlen(src);
					for (int i = 0; i < tmp_len && src[i] != '%'; i++) {
						field_name[i] = src[i];
						++src;
					}
					field_name[tmp_len] = '\0';
					field_num = (int) strtol(field_name, &field_chk, 10);
					if (*field_chk != '\0') {
						ERROR("Invalid field format, integer conversion failed");
					}

					getFieldFromInputFile(ip_file, field_num, nullptr, field_ptr);
					count += snprintf(&call_id[count], MAX_HEADER_LEN-count-1, "%s", field_buff);
					break;
				default:      // treat all unknown sequences as %%
					call_id[count++] = '%';
					break;
			}
		} else {
			call_id[count++] = *src++;
		}
	}
	call_id[count] = 0;

	return call_id;
}

void listener::getFieldFromInputFile(const char *fileName, int field, SendingMessage *lineMsg, char*& dest)
{
	if (m_lineNumber == nullptr) {
		ERROR("Automatic calls (created by -aa, -oocsn or -oocsf) cannot use input files!");
	}
	if (inFiles.find(fileName) == inFiles.end()) {
		ERROR("Invalid injection file: %s", fileName);
	}
	int line = (*m_lineNumber)[fileName];
	// WARNING: this branch should never be taken from the base class
	if (lineMsg) {
		char lineBuffer[20];
		char *endptr;
		createSendingMessage(lineMsg, SM_UNUSED, lineBuffer, sizeof(lineBuffer));
		line = (int) strtod(lineBuffer, &endptr);
		if (*endptr != 0) {
			ERROR("Invalid line number generated: '%s'", lineBuffer);
		}
		if (line > inFiles[fileName]->numLines()) {
			line = -1;
		}
	}
	if (line < 0) {
		return;
	}
	dest += inFiles[fileName]->getField(line, field, dest, SIPP_MAX_MSG_SIZE);
}
