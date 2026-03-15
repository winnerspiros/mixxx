#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QFileInfo>

#include "test/mixxxtest.h"

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

static std::vector<std::string> SplitMappings(const std::string& s) {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, '|')) {
        if (!item.empty()) res.push_back(item);
    }
    return res;
}

class MappingTestFixture : public MixxxTest, public ::testing::WithParamInterface<std::string> {
};

TEST_P(MappingTestFixture, ValidateXml) {
    QString path = QString::fromStdString(GetParam());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));

    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
    }
    EXPECT_FALSE(xml.hasError()) << "XML error in " << path.toStdString() << ": " << xml.errorString().toStdString();
}

static std::string PrintMappingName(const ::testing::TestParamInfo<std::string>& info) {
    QFileInfo fileInfo(QString::fromStdString(info.param));
    std::string name = fileInfo.baseName().toStdString();
    std::replace(name.begin(), name.end(), '.', '_');
    std::replace(name.begin(), name.end(), '-', '_');
    return name;
}

#if !defined(__ANDROID__)
#ifdef CONTROLLER_BULK_MAPPINGS
INSTANTIATE_TEST_SUITE_P(BulkMappings,
        MappingTestFixture,
        ::testing::ValuesIn(SplitMappings(CONTROLLER_BULK_MAPPINGS)),
        PrintMappingName);
#endif

#ifdef CONTROLLER_HID_MAPPINGS
INSTANTIATE_TEST_SUITE_P(HidMappings,
        MappingTestFixture,
        ::testing::ValuesIn(SplitMappings(CONTROLLER_HID_MAPPINGS)),
        PrintMappingName);
#endif

#ifdef CONTROLLER_MIDI_MAPPINGS
INSTANTIATE_TEST_SUITE_P(MidiMappings,
        MappingTestFixture,
        ::testing::ValuesIn(SplitMappings(CONTROLLER_MIDI_MAPPINGS)),
        PrintMappingName);
#endif
#endif

#include "moc_controller_mapping_validation_test.cpp"
