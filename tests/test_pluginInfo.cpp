#include <gtest/gtest.h>
#include <plugin_api.h>
#include <rapidjson/document.h>

#include "version.h"


extern "C" {
	PLUGIN_INFORMATION *plugin_info(); 
};

TEST(TestPluginInfo, PluginInfo)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ASSERT_STREQ(info->name, "systemspn");
	ASSERT_STREQ(info->version, VERSION);
	ASSERT_EQ(info->options, SP_INGEST);
	ASSERT_STREQ(info->type, PLUGIN_TYPE_NOTIFICATION_DELIVERY);
	ASSERT_STREQ(info->interface, "1.0.0");
}

TEST(TestPluginInfo, PluginInfoConfigParse)
{
	PLUGIN_INFORMATION *info = plugin_info();
	rapidjson::Document doc;
	doc.Parse(info->config);
	ASSERT_EQ(doc.HasParseError(), false);
	ASSERT_EQ(doc.IsObject(), true);
	ASSERT_EQ(doc.HasMember("plugin"), true);
	ASSERT_EQ(doc.HasMember("enable"), true);
	ASSERT_EQ(doc.HasMember("exchanged_data"), true);
}