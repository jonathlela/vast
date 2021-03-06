
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007-2008 Shao-Chen Chang (cscxcs at gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 *  attributbuilder.h -- VASTATE Simulator - SimGame attributes builder
 *
 */

#ifndef _ATTRIBUTE_BUILDER_H
#define _ATTRIBUTE_BUILDER_H

#include "shared.h"

using namespace VAST;

#include <string>
#include <sstream>

namespace SimGame
{
	// Object Type
	const int OT_PLAYER   = 1;
	const int OT_FOOD     = 2;

	// Event Type
	const int E_MOVE   = 1;
	const int E_ATTACK = 2;
	const int E_EAT    = 3;
	const int E_BOMB   = 4;

	// Index of attribute(s)
	const int ObjectType  = 0;
	// Player
	const int PlayerName  = 1;
	const int PlayerHP    = 2;
	const int PlayerMaxHP = 3;
	// Food
	const int FoodCount   = 1;
	//

}  /* namespace SimGame */

class AttributeBuilder
{
public:
	AttributeBuilder ()
	{
	}

	~AttributeBuilder ()
	{
	}

	//
	//	Object Type
	//
	int getType (object & obj)
	{
		int otype;
		obj.get (SimGame::ObjectType, otype);
		return otype;
	}

	bool checkType (object& obj, int type)
	{
		return (getType(obj) == type);
		//return (obj.type == type);
		//return true;
	}

    string objectIDToString (obj_id_t obj_id)
    {
        std::stringstream ssout;
        ssout
        << std::hex
        << "[" << (obj_id >> 16) << "_" << (obj_id & 0xFFFF) << "]";
        return ssout.str ();
    }

    string objectIDToString (object &obj)
    {
        return objectIDToString (obj.get_id ());
    }

	string objectToString (object &obj)
	{
		std::stringstream ssout;
		int otype;

        ssout   << objectIDToString (obj);

		if (obj.get (0, otype) == false)
        {
            ssout   << "(" << (int) obj.get_pos().x << "," << (int) obj.get_pos().y << ")"
                    << "<v" << obj.pos_version << "> "
                    << "(Type inexist!) "
                    << "<v" << obj.version << "> ";
        }
        else
        {
		    switch (otype)
		    {
			    case SimGame::OT_PLAYER:
				    ssout	
					    << "[" << obj.peer << "]"
					    << "(" << (int) obj.get_pos().x << "," << (int) obj.get_pos().y << ")"
					    << "<v" << obj.pos_version << ">"
					    << "  "
					    << "\"" << getPlayerName (obj) << "\" "
					    << getPlayerHP(obj) << "/" << getPlayerMaxHP(obj)
					    << "<v" << obj.version << ">";
				    break;
			    case SimGame::OT_FOOD:
				    ssout	
					    << "(" << (int) obj.get_pos().x << "," << (int) obj.get_pos().y << ")"
					    << "<v" << obj.pos_version << ">"
					    << "  "
					    << "Count: " << getFoodCount (obj)
					    << "<v" << obj.version << ">";
				    break;
			    default:
				    ssout   
                        << "(" << (int) obj.get_pos().x << "," << (int) obj.get_pos().y << ")"
					    << "<v" << obj.pos_version << "> "
                        << "  "
                        << " type: " << getType (obj) << " "
					    << "<v" << obj.version << "> "
					    << "Unknown Object";
		    }
        }

		return ssout.str();
	}

    // Warning: this function returns *a new object* is equal to source_obj, MUST free it after using.
    object * copyObject (object & source_obj)
    {
        object * nobj = new object (source_obj.get_id ());
        nobj->set_pos (source_obj.get_pos ());
        nobj->peer = source_obj.peer;
        nobj->pos_version = source_obj.pos_version;
        nobj->version = source_obj.version;
        switch (getType (source_obj))
        {
            case SimGame::OT_PLAYER:
                buildPlayer (*nobj, getPlayerName (source_obj), getPlayerHP (source_obj), getPlayerMaxHP (source_obj));
                break;
            case SimGame::OT_FOOD:
                buildFood (*nobj, getFoodCount (source_obj));
                break;
            default:
                printf ("AttribteBuilder::copyObject: received unknown source object.\n");
        }
        return nobj;
    }


	//
	//  Player
	//
	bool buildPlayer (object & obj, 
						string name, int hp, int maxhp
						)
	{
		bool ret = true;

		obj.add (SimGame::OT_PLAYER);
		obj.add (name);
		obj.add (hp);
		obj.add (maxhp);

		return ret;
	}

	string getPlayerName (object& obj)
	{
		string str;
		return (checkType(obj,SimGame::OT_PLAYER) && obj.get (SimGame::PlayerName, str))?str:"";
	}

	int getPlayerHP (object& obj)
	{
		int value;
		return (checkType(obj,SimGame::OT_PLAYER) && obj.get (SimGame::PlayerHP, value))?value:0;
	}

	int setPlayerHP (object& obj, int newhp)
	{
		int currentvalue;
		if (checkType(obj,SimGame::OT_PLAYER) && obj.get (SimGame::PlayerHP, currentvalue))
		{
			obj.set (SimGame::PlayerHP, newhp);
			return currentvalue;
		}
		return -1;
	}

	int getPlayerMaxHP (object& obj)
	{
		int value;
		return (checkType(obj,SimGame::OT_PLAYER) && obj.get (SimGame::PlayerMaxHP, value))?value:0;
	}
	////

	//
	//	Food
	//
	bool buildFood (object & obj, 
						int count
						)
	{
		bool ret=true;
		obj.add ((int) SimGame::OT_FOOD);
		obj.add ((int) count);
		return ret;
	}

	int getFoodCount (object& obj)
	{
		int value;
		return (checkType(obj,SimGame::OT_FOOD) && obj.get (SimGame::FoodCount, value))?value:0;
	}
	int setFoodCount (object& obj, int newValue)
	{
		int curvalue;
		if (checkType(obj,SimGame::OT_FOOD) && obj.get (SimGame::FoodCount, curvalue))
		{
			obj.set (SimGame::FoodCount, newValue);
			return curvalue;
		}
		return -1;
	}
};

#endif /* _ATTRIBUTE_BUILDER_H */

