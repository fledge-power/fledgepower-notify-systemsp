#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <plugin_api.h>

#include "notifySystemSp.h"

using namespace systemspn;

extern "C" {
	PLUGIN_INFORMATION *plugin_info();
	PLUGIN_HANDLE plugin_init(ConfigCategory *config);
    void plugin_shutdown(PLUGIN_HANDLE *handle);
};

class TestPluginShutdown : public testing::Test
{
protected:
    NotifySystemSp *filter = nullptr;  // Object on which we call for tests

    // Setup is ran for every tests, so each variable are reinitialised
    void SetUp() override
    {
        PLUGIN_INFORMATION *info = plugin_info();
		ConfigCategory *config =  new ConfigCategory("systemsp", info->config);
		ASSERT_NE(config, nullptr);		
		config->setItemsValueFromDefault();
		config->setValue("enable", "true");
		
		PLUGIN_HANDLE handle = nullptr;
        ASSERT_NO_THROW(handle = plugin_init(config));
		filter = static_cast<NotifySystemSp *>(handle);
        ASSERT_EQ(filter->isEnabled(), true);
    }
};

TEST_F(TestPluginShutdown, Shutdown) 
{
	if (filter) {
        ASSERT_NO_THROW(plugin_shutdown(reinterpret_cast<PLUGIN_HANDLE*>(filter)));
    }
}