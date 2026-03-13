#include <QList>
#include <QObject>
#include <QtDebug>
#include <QtTest>

#include "control/pollingcontrolproxy.h"
#include "effects/backends/effectmanifest.h"
#include "effects/effectsmanager.h"
#include "test/signalpathtest.h"


class StemControlFixture : public SignalPathTest {
  public:
    StemControlFixture() : SignalPathTest() {
    }

    void SetUp() override {
        SignalPathTest::SetUp();
    }

    QString getGroupForStem(QStringView deckGroup, int stemNr) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return deckGroup.chopped(1).toString() + QStringLiteral("_Stem") + QString::number(stemNr) + QStringLiteral("]");
#else
        return deckGroup.chopped(1) + QStringLiteral("_Stem") + QChar('0' + stemNr) + QChar(']');
#endif
    }
};

class StemControlTest : public StemControlFixture {
};

TEST_F(StemControlTest, InitialValue) {
    for (int i = 0; i < 4; ++i) {
        QString group = getGroupForStem(QStringLiteral("[Channel1]"), i);
        ControlProxy p(group, QStringLiteral("enabled"));
        ASSERT_TRUE(p.valid());
        EXPECT_DOUBLE_EQ(1.0, p.get());
    }
}
