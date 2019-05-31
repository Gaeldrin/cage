#include <exception>

#include <cage-core/core.h>
#include <cage-core/log.h>
#include <cage-core/math.h>
#include <cage-core/image.h>
#include <cage-core/ini.h>

using namespace cage;

void separate(holder<iniClass> &cmd)
{
	string names[4] = { "1.png", "2.png", "3.png", "4.png" };
	string input = "input.png";
	for (const string &option : cmd->sections())
	{
		if (option == "1" || option == "2" || option == "3" || option == "4")
		{
			if (cmd->itemsCount(option) != 1)
			{
				CAGE_LOG(severityEnum::Note, "exception", string() + "option: '" + option + "'");
				CAGE_THROW_ERROR(exception, "option expects one argument");
			}
			uint32 index = option.toUint32() - 1;
			string name = cmd->get(option, "0");
			names[index] = name;
		}
		else if (option == "i" || option == "input")
		{
			if (cmd->itemsCount(option) != 1)
			{
				CAGE_LOG(severityEnum::Note, "exception", string() + "option: '" + option + "'");
				CAGE_THROW_ERROR(exception, "option expects one argument");
			}
			input = cmd->get(option, "0");
		}
	}

	CAGE_LOG(severityEnum::Info, "image", string() + "loading image: '" + input + "'");
	holder<imageClass> in = newImage();
	in->decodeFile(input);
	uint32 width = in->width();
	uint32 height = in->height();
	CAGE_LOG(severityEnum::Info, "image", string() + "image resolution: " + width + "x" + height + ", channels: " + in->channels());

	holder<imageClass> out = newImage();
	for (uint32 ch = 0; ch < in->channels(); ch++)
	{
		if (names[ch].empty())
			continue;
		out->empty(width, height, 1);
		for (uint32 y = 0; y < height; y++)
		{
			for (uint32 x = 0; x < width; x++)
				out->value(x, y, 0, in->value(x, y, ch));
		}
		CAGE_LOG(severityEnum::Info, "image", string() + "saving image: '" + names[ch] + "'");
		out->encodeFile(names[ch]);
	}
	CAGE_LOG(severityEnum::Info, "image", string() + "ok");
}

void combine(holder<iniClass> &cmd)
{
	holder<imageClass> pngs[4];
	uint32 width = 0, height = 0;
	uint32 channels = 0;
	string output = "combined.png";
	for (const string &option : cmd->sections())
	{
		if (option == "1" || option == "2" || option == "3" || option == "4")
		{
			if (cmd->itemsCount(option) != 1)
			{
				CAGE_LOG(severityEnum::Note, "exception", string() + "option: '" + option + "'");
				CAGE_THROW_ERROR(exception, "option expects one argument");
			}
			uint32 index = option.toUint32() - 1;
			string name = cmd->get(option, "0");
			CAGE_LOG(severityEnum::Info, "image", string() + "loading image: '" + name + "' for " + (index + 1) + "th channel");
			holder<imageClass> p = newImage();
			p->decodeFile(name);
			CAGE_LOG(severityEnum::Info, "image", string() + "image resolution: " + p->width() + "x" + p->height() + ", channels: " + p->channels());
			if (width == 0)
			{
				width = p->width();
				height = p->height();
			}
			else
			{
				if (p->width() != width || p->height() != height)
					CAGE_THROW_ERROR(exception, "image resolution does not match");
			}
			if (p->channels() != 1)
				CAGE_THROW_ERROR(exception, "the image has to be mono channel");
			channels = max(channels, index + 1u);
			pngs[index] = templates::move(p);
		}
		else if (option == "o" || option == "output")
		{
			if (cmd->itemsCount(option) != 1)
			{
				CAGE_LOG(severityEnum::Note, "exception", string() + "option: '" + option + "'");
				CAGE_THROW_ERROR(exception, "option expects one argument");
			}
			output = cmd->get(option, "0");
		}
		else
		{
			CAGE_LOG(severityEnum::Note, "exception", string() + "option: '" + option + "'");
			CAGE_THROW_ERROR(exception, "unknown option");
		}
	}
	if (channels == 0)
		CAGE_THROW_ERROR(exception, "no inputs specified");

	CAGE_LOG(severityEnum::Info, "image", string() + "combining image");
	holder<imageClass> res = newImage();
	res->empty(width, height, channels);
	for (uint32 i = 0; i < channels; i++)
	{
		holder<imageClass> &src = pngs[i];
		if (!src)
			continue;
		for (uint32 y = 0; y < height; y++)
		{
			for (uint32 x = 0; x < width; x++)
				res->value(x, y, i, src->value(x, y, 0));
		}
	}

	CAGE_LOG(severityEnum::Info, "image", string() + "saving image: '" + output + "'");
	res->encodeFile(output);
	CAGE_LOG(severityEnum::Info, "image", string() + "ok");
}

int main(int argc, const char *args[])
{
	try
	{
		holder<loggerClass> log1 = newLogger();
		log1->format.bind<logFormatPolicyConsole>();
		log1->output.bind<logOutputPolicyStdOut>();

		holder<iniClass> cmd = newIni();
		cmd->parseCmd(argc, args);

		for (const string &option : cmd->sections())
		{
			if (option == "s" || option == "separate")
			{
				if (cmd->itemsCount(option) != 1)
				{
					CAGE_LOG(severityEnum::Note, "exception", string() + "option: '" + option + "'");
					CAGE_THROW_ERROR(exception, "option expects one argument");
				}
				if (cmd->get(option, "0").toBool())
				{
					separate(cmd);
					return 0;
				}
			}
		}

		combine(cmd);
		return 0;
	}
	catch (const cage::exception &)
	{
	}
	catch (const std::exception &e)
	{
		CAGE_LOG(severityEnum::Error, "exception", string() + "std exception: " + e.what());
	}
	catch (...)
	{
		CAGE_LOG(severityEnum::Error, "exception", "unknown exception");
	}
	return 1;
}
