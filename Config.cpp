#include <stdlib.h>
#include <filesystem>

#include "Config.h"
#include "GameApi.h"
#include <string>

namespace fs = std::filesystem;

namespace Config
{
	std::vector<Car*> carList;
	Car* GetCar(int carId)
	{
		char* carName = Game::GetCarTypeName(carId);

		for (Car* i : carList)
		{
			if (carName == i->_Name)
			{
				return i;
			}
		}

		return NULL;
	}

	Car* GetCarByHash(int hash)
	{
		for (Car* i : carList)
		{
			if (hash == Game::StringHash(i->_Name.c_str()))
			{
				return i;
			}
		}

		return NULL;
	}

	Car* GetCarByName(std::string name)
	{
		for (Car* i : carList)
		{
			if (name == i->_Name)
			{
				return i;
			}
		}

		return NULL;
	}

	Global* glc = NULL;
	Car* Get(int carId)
	{
		auto config = GetCar(carId);
		if (config)
		{
			return config;
		}

		return glc;
	}

	Car* GetByHash(int hash)
	{
		auto config = GetCarByHash(hash);
		if (config)
		{
			return config;
		}

		return glc;
	}

	Global* GetGlobal()
	{
		return glc;
	}

	_State InitState(int value)
	{
		if (value < -1 || value > 1)
		{
			return _State::DefaultState;
		}

		return (_State)value;
	}

	Part* Car::GetPart(DBPart::_DBPart dbpart)
	{
		for (auto& part : this->Parts)
		{
			if (part.DBPart == dbpart)
			{
				return &part;
			}
		}

		return NULL;
	}

	int GetPartHeader(int carId, DBPart::_DBPart dbpart, bool isAS)
	{
		auto carConfig = GetCar(carId);
		if (carConfig)
		{
			auto part = carConfig->GetPart(dbpart);
			if (part)
			{
				int header = isAS ? part->HeaderAS : part->Header;
				if (header != -1)
				{
					return header;
				}
			}
		}

		auto part = glc->GetPart(dbpart);
		if (part)
		{
			return isAS ? part->HeaderAS : part->Header;
		}

		return -1;
	}

	_State GetPartState(int carId, DBPart::_DBPart dbpart)
	{
		auto carConfig = GetCar(carId);
		if (carConfig)
		{
			auto part = carConfig->GetPart(dbpart);
			if (part)
			{
				if (part->State != _State::DefaultState)
				{
					return part->State;
				}
			}
		}

		auto part = glc->GetPart(dbpart);
		if (part)
		{
			return part->State;
		}

		return _State::DisabledState;
	}

	_State GetSetting(int carId, Settings::_Settings setting)
	{
		auto carConfig = GetCar(carId);
		if (carConfig)
		{
			auto st = carConfig->Settings[setting];
			if (st != _State::DefaultState)
			{
				return st;
			}
		}

		return glc->Settings[setting];
	}

	char* GetPartCamera(int carId, DBPart::_DBPart dbpart)
	{
		auto carConfig = GetCar(carId);
		if (carConfig)
		{
			auto part = carConfig->GetPart(dbpart);
			if (part)
			{
				if (part->Camera)
				{
					return part->Camera;
				}
			}
		}

		auto part = glc->GetPart(dbpart);
		if (part)
		{
			return part->Camera;
		}

		return NULL;
	}
}