#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

#include "test/mixxxtest.h"

class MappingTestFixture : public MixxxTest, public ::testing::WithParamInterface<const char*> {
};

TEST_P(MappingTestFixture, validateXml) {
    QString path = GetParam();
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));

    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
    }
    EXPECT_FALSE(xml.hasError()) << "XML error in " << path.toStdString() << ": " << xml.errorString().toStdString();
}

static std::string PrintMappingName(const ::testing::TestParamInfo<const char*>& info) {
    QFileInfo fileInfo(info.param);
    std::string name = fileInfo.baseName().toStdString();
    std::replace(name.begin(), name.end(), '.', '_');
    std::replace(name.begin(), name.end(), '-', '_');
    return name;
}

#if !defined(__ANDROID__)
#ifdef CONTROLLER_BULK_MAPPINGS
INSTANTIATE_TEST_SUITE_P(BulkMappings,
        MappingTestFixture,
        ::testing::Values(CONTROLLER_BULK_MAPPINGS),
        PrintMappingName);
#endif

#ifdef CONTROLLER_HID_MAPPINGS
INSTANTIATE_TEST_SUITE_P(HidMappings,
        MappingTestFixture,
        ::testing::Values(CONTROLLER_HID_MAPPINGS),
        PrintMappingName);
#endif

#ifdef CONTROLLER_MIDI_MAPPINGS
INSTANTIATE_TEST_SUITE_P(MidiMappings,
        MappingTestFixture,
        ::testing::Values(CONTROLLER_MIDI_MAPPINGS),
        PrintMappingName);
#endif
#endif
