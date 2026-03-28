#include <gtest/gtest.h>
#include "util/cmdlineargs.h"

namespace mixxx {

class CmdlineArgsTest : public ::testing::Test {
};

TEST_F(CmdlineArgsTest, DefaultQmlOnAndroid) {
    CmdlineArgs args;
#ifdef Q_OS_ANDROID
    EXPECT_TRUE(args.isQml());
#else
    EXPECT_FALSE(args.isQml());
#endif
}

TEST_F(CmdlineArgsTest, OverrideNoNewUi) {
    CmdlineArgs args;
    QStringList arguments = {"mixxx", "--no-new-ui"};
    args.parse(arguments, CmdlineArgs::ParseMode::Initial);
    EXPECT_FALSE(args.isQml());
}

TEST_F(CmdlineArgsTest, OverrideNewUi) {
    CmdlineArgs args;
    QStringList arguments = {"mixxx", "--new-ui"};
    args.parse(arguments, CmdlineArgs::ParseMode::Initial);
    EXPECT_TRUE(args.isQml());
}

} // namespace mixxx
