#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <plugin_api.h>

#include "notifySystemSp.h"

using namespace systemspn;


static std::string reconfigure = QUOTE({
    "enable": {
        "value": "false"
    }
});

extern "C" {
	PLUGIN_INFORMATION *plugin_info();
	PLUGIN_HANDLE plugin_init(ConfigCategory *config);
    void plugin_reconfigure(PLUGIN_HANDLE *handle, const std::string& newConfig);
    void plugin_shutdown(PLUGIN_HANDLE *handle);
};

class TestPluginReconfigure : public testing::Test
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

    // TearDown is ran for every tests, so each variable are destroyed again
    void TearDown() override
    {
        if (filter) {
            ASSERT_NO_THROW(plugin_shutdown(reinterpret_cast<PLUGIN_HANDLE*>(filter)));
        }
    }   
};

TEST_F(TestPluginReconfigure, Reconfigure) 
{
	plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), reconfigure);
    ASSERT_EQ(filter->isEnabled(), false);
}