// SPDX-License-Identifier: Apache-2.0
// Cross-repository CF2 acceptance: real custom libraries, editor metadata and
// the same SimulationManager used by the application. Artifacts stay in-tree.
#include "core/composerapplication.h"
#include "plugins/componentregistry.h"
#include "project/componentinstances.h"
#include "project/compositiondocument.h"
#include "simulation/simulationmanager.h"
#include <hydrocouplesdk/component/abstractmodelcomponent.h>
#include <gtest/gtest.h>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
using namespace HydroCouple;
using namespace HydroCouple::Composer;
using Json=nlohmann::json;
namespace {
Json config() {
    return {{"schema","hydrocouple.interval-transfer/1"},{"epoch_julian_day",2440587.5},
      {"species",Json::array({{{"id","NH4_N"},{"basis","kg-N"}},{{"id","NO3_N"},{"basis","kg-N"}}})}};
}
Json sourceConfig() {
    auto j=config(); j["intervals"]=Json::array({
      {{"start_s",0},{"end_s",60},{"water_m3",2},{"enthalpy_j",100},{"species",{{"NH4_N",.2},{"NO3_N",.1}}}},
      {{"start_s",60},{"end_s",90},{"water_m3",3},{"enthalpy_j",150},{"species",{{"NH4_N",.3},{"NO3_N",.15}}}}});
    return j;
}
class CustomComponents:public ::testing::Test {
protected:
    static ComposerApplication* app;
    ComponentRegistry registry;
    QDir directory;
    static void SetUpTestSuite() {
        static int argc=1; static char name[]="test_customcomponents"; static char* argv[]={name,nullptr};
        if(!qApp)app=new ComposerApplication(argc,argv);
    }
    static void TearDownTestSuite() { delete app; app=nullptr; }
    void SetUp() override {
        directory=QDir(QStringLiteral(COMPOSER_CUSTOM_ARTIFACT_DIR)).filePath(::testing::UnitTest::GetInstance()->current_test_info()->name());
        ASSERT_TRUE(QDir().mkpath(directory.path()));
        QString message;
        for(const auto& path:{QStringLiteral(COMPOSER_INTERVAL_SOURCE_LIBRARY),QStringLiteral(COMPOSER_INTERVAL_RECEIVER_LIBRARY),QStringLiteral(COMPOSER_OPENSWMM_COMPONENT_LIBRARY)})
            ASSERT_NE(registry.loadLibrary(path,message),nullptr)<<message.toStdString();
#ifdef COMPOSER_OPENSWMM_ENGINE_COMPONENT_LIBRARY
        ASSERT_NE(registry.loadLibrary(QStringLiteral(COMPOSER_OPENSWMM_ENGINE_COMPONENT_LIBRARY),message),nullptr)<<message.toStdString();
#endif
    }
    Json receiverConfig(const std::string& name="receipts.csv") {
        auto j=config(); j["expected_transfers"]=2;
        j["receipt_file"]=directory.filePath(QString::fromStdString(name)).toStdString(); return j;
    }
    Json composition() {
        auto receiver=receiverConfig(); std::reverse(receiver["species"].begin(),receiver["species"].end());
        Json j={{"components",Json::array({
          {{"id","source"},{"info",{{"component_info_id","org.hydrocouple.openswmm.test.interval-source"}}},{"arguments",{{"config",sourceConfig()}}}},
          {{"id","receiver"},{"info",{{"component_info_id","org.hydrocouple.openswmm.test.sink"}}},{"arguments",{{"config",receiver}}}}
        })},{"connections",Json::array()},{"workflow",{{"strategy","time_stepped"}}}};
        for(const std::string field:{"transfer_id","epoch_julian_day","interval_start_s","interval_end_s","water_m3","enthalpy_j","species.NH4_N","species.NO3_N"})
            j["connections"].push_back({{"from",{{"component","source"},{"output",field}}},{"to",{{"component","receiver"},{"input",field}}}});
        return j;
    }
    void save(const Json& j) {
        std::ofstream file(directory.filePath("composition.json").toStdString()); file<<j.dump(2)<<'\n';
    }
};
ComposerApplication* CustomComponents::app=nullptr;
}
TEST_F(CustomComponents, TwoSdkFreeEngineInstancesReceiveStableDocumentIdentities) {
    Json j={{"components",Json::array({
      {{"id","network-one"},{"info",{{"component_info_id","openswmm.model"}}}},
      {{"id","network-two"},{"info",{{"component_info_id","openswmm.model"}}}}
    })}};
    CompositionDocument document; QString message;
    ASSERT_TRUE(document.loadFromJson(QByteArray::fromStdString(j.dump()),message))<<message.toStdString();
    ComponentInstances instances(&document,&registry);
    auto* first=instances.instance("network-one"); auto* second=instances.instance("network-two");
    ASSERT_NE(first,nullptr)<<instances.failure("network-one").toStdString(); ASSERT_NE(second,nullptr);
    EXPECT_EQ(first->id(),"network-one"); EXPECT_EQ(second->id(),"network-two"); EXPECT_NE(first,second);
    EXPECT_EQ(dynamic_cast<SDK::AbstractModelComponent*>(first),nullptr);
    EXPECT_EQ(first->componentInfo()->id(),"openswmm.model");
    EXPECT_FALSE(ComponentRegistry::assignInstanceIdentity(*first,"renamed",message));
    EXPECT_EQ(first->id(),"network-one");
    for(auto* arg:first->arguments())if(arg->id()=="hydrocouple.instance_id") {
        std::string error,value;
        ASSERT_TRUE(arg->serialize(IArgument::ArgumentInputType::JSON,value,error));
        EXPECT_EQ(Json::parse(value),Json({{"values",{"network-one"}}}));
        EXPECT_FALSE(arg->initialize("other",IArgument::ArgumentInputType::String,error));
    }
}
TEST_F(CustomComponents, IdentityArgumentRefusesEmptyOrChangedIdentityBeforeInitialization) {
    QString message; auto component=registry.createInstance("openswmm.model",message); ASSERT_NE(component,nullptr);
    EXPECT_FALSE(ComponentRegistry::assignInstanceIdentity(*component,"",message));
    ASSERT_TRUE(ComponentRegistry::assignInstanceIdentity(*component,"native-a",message))<<message.toStdString();
    EXPECT_FALSE(ComponentRegistry::assignInstanceIdentity(*component,"native-b",message));
    EXPECT_EQ(component->id(),"native-a");
}
TEST_F(CustomComponents, ConfiguredCatalogueRefreshesAfterAnEditWithoutCreatingReceipts) {
    auto j=composition(); const auto receipt=directory.filePath("receipts.csv"); QFile::remove(receipt);
    save(j); CompositionDocument document; QString message;
    ASSERT_TRUE(document.loadFromJson(QByteArray::fromStdString(j.dump()),message));
    ComponentInstances instances(&document,&registry);
    auto* initial=instances.instance("receiver"); ASSERT_NE(initial,nullptr)<<instances.failure("receiver").toStdString();
    EXPECT_EQ(initial->id(),"receiver"); EXPECT_EQ(instances.inputs("receiver").size(),8);
    auto changed=receiverConfig(); changed["species"].erase(1);
    ASSERT_TRUE(document.setArgument("receiver","config",changed));
    auto* edited=instances.instance("receiver"); ASSERT_NE(edited,nullptr); EXPECT_NE(edited,initial);
    EXPECT_EQ(instances.inputs("receiver").size(),7); EXPECT_FALSE(QFile::exists(receipt));
}
TEST_F(CustomComponents, ConflictingIdentityArgumentFailsBeforeSimulation) {
    Json j={{"components",Json::array({
      {{"id","network"},{"info",{{"component_info_id","openswmm.model"}}},
       {"arguments",{{"hydrocouple.instance_id",{{"values",{"different"}}}}}}}
    })}};
    save(j); CompositionDocument document; QString message;
    ASSERT_TRUE(document.loadFromJson(QByteArray::fromStdString(j.dump()),message));
    SimulationManager manager(&registry);
    EXPECT_FALSE(manager.start(document,message)); EXPECT_FALSE(message.isEmpty()); EXPECT_FALSE(manager.isRunning());
}
TEST_F(CustomComponents, RunsCustomSourceAndReceiverThroughTheApplicationWorkflow) {
    auto j=composition(); save(j); CompositionDocument document; QString message;
    ASSERT_TRUE(document.loadFromJson(QByteArray::fromStdString(j.dump()),message))<<message.toStdString();
    SimulationManager manager(&registry); QSignalSpy finished(&manager,&SimulationManager::finished);
    ASSERT_TRUE(manager.start(document,message))<<message.toStdString();
    QElapsedTimer timer; timer.start();
    while(finished.empty() && timer.elapsed()<15000)QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    ASSERT_FALSE(finished.empty())<<manager.errors().join("; ").toStdString();
    EXPECT_TRUE(finished.first()[0].toBool())<<finished.first()[1].toString().toStdString()<<manager.errors().join("; ").toStdString();
    EXPECT_EQ(manager.state(),SimulationState::Finished);
    QFile file(directory.filePath("receipts.csv")); ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const auto lines=file.readAll().trimmed().split('\n'); ASSERT_EQ(lines.size(),3);
    EXPECT_TRUE(lines[1].contains("\"receiver\",\"source:1\""));
    const auto last=lines[2].split(','); EXPECT_EQ(last[6].toDouble(),3); // accepted interval water
    EXPECT_EQ(last[9].toDouble(),150); // accepted interval heat
}
TEST_F(CustomComponents, RelativeReceiptsUseTheSavedCompositionDirectory) {
    auto j=composition();
    j["components"][1]["arguments"]["config"]["receipt_file"]="relative-custom-receipts/receipts.csv";
    const auto expected=directory.filePath("relative-custom-receipts/receipts.csv"); QFile::remove(expected);
    CompositionDocument document; QString message;
    ASSERT_TRUE(document.loadFromJson(QByteArray::fromStdString(j.dump()),message));
    ASSERT_TRUE(document.save(directory.filePath("composition.json"),message))<<message.toStdString();
    SimulationManager manager(&registry); QSignalSpy finished(&manager,&SimulationManager::finished);
    ASSERT_TRUE(manager.start(document,message))<<message.toStdString();
    QElapsedTimer timer; timer.start();
    while(finished.empty() && timer.elapsed()<15000)QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    ASSERT_FALSE(finished.empty()); ASSERT_TRUE(finished.first()[0].toBool())<<finished.first()[1].toString().toStdString();
    EXPECT_TRUE(QFile::exists(expected))<<expected.toStdString();
}
TEST_F(CustomComponents, LegacyConfigurationCannotOverwriteTheHostIdentity) {
    QString message; auto component=registry.createInstance("openswmm.model",message); ASSERT_NE(component,nullptr);
    ASSERT_TRUE(ComponentRegistry::assignInstanceIdentity(*component,"network",message));
    const Json configuration={{"schema","openswmm.reservoir-coupling/1.0"},{"role","network_only"},
      {"ownership","swmm-network/fvqual-receiver"},{"epoch_julian_day",2440587.5},{"instance_id","other"}};
    for(auto* argument:component->arguments())if(argument->id()=="configuration") {
        std::string error;
        ASSERT_TRUE(argument->initialize(configuration.dump(),IArgument::ArgumentInputType::JSON,error))<<error;
    }
    component->initialize();
    const auto errors=component->validate(); ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.front().find("conflicts"),std::string::npos); EXPECT_EQ(component->id(),"network");
}

#ifdef COMPOSER_OPENSWMM_ENGINE_COMPONENT_LIBRARY
TEST_F(CustomComponents, NativeEngineMetadataUsesTheConfiguredModelWithoutStartingIt) {
    const auto model=directory.filePath("model.inp"); QFile::remove(model);
    ASSERT_TRUE(QFile::copy(QStringLiteral(COMPOSER_ENGINE_COMPONENT_FIXTURE),model));
    const auto result=directory.filePath("native-results"); QDir(result).removeRecursively();
    const Json configuration={{"schema","openswmm.component/1"},{"role","complete_model"},
        {"model","model.inp"},{"output_directory","native-results"}};
    Json j={{"components",Json::array({{{"id","network"},
        {"info",{{"component_info_id","org.hydrocouple.openswmm.engine"}}},
        {"arguments",{{"configuration",configuration}}}}})}};
    save(j); CompositionDocument document; QString message;
    ASSERT_TRUE(document.load(directory.filePath("composition.json"),message))<<message.toStdString();
    ComponentInstances instances(&document,&registry);
    auto* engine=instances.instance("network");ASSERT_NE(engine,nullptr)<<instances.failure("network").toStdString();
    EXPECT_EQ(dynamic_cast<SDK::AbstractModelComponent*>(engine),nullptr);
    IOutput* metadata=nullptr;
    for(auto* item:engine->outputs())if(item->id()=="model_metadata")metadata=item;
    ASSERT_NE(metadata,nullptr);
    const int64_t one=1,zero=0;std::string text,error;
    BufferDescriptor buffer{&text,DataKind::String,1,&one,nullptr,sizeof(text)};
    ASSERT_TRUE(metadata->getValuesInto(buffer,{&zero,1},{&one,1},&error))<<error;
    const auto parsed=Json::parse(text);
    EXPECT_EQ(parsed["nodes"].size(),2u);EXPECT_EQ(parsed["links"].size(),1u);
    EXPECT_EQ(parsed["pollutants"][1]["concentration_units"],"ug/L");
    EXPECT_EQ(parsed["instance_id"],"network");EXPECT_TRUE(parsed["runnable"]);
    EXPECT_FALSE(QFile::exists(result));
}
TEST_F(CustomComponents, NativeOutfallFlowsThroughTheApplicationIntoTheCustomReceiver) {
    const auto model=directory.filePath("model.inp");QFile::remove(model);
    ASSERT_TRUE(QFile::copy(QStringLiteral(COMPOSER_ENGINE_COMPONENT_FIXTURE),model));
    QDir(directory.filePath("native-results")).removeRecursively();
    QFile::remove(directory.filePath("native-receipts.csv"));
    const Json configuration={{"schema","openswmm.component/1"},{"role","complete_model"},
        {"model","model.inp"},{"output_directory","native-results"},
        {"outfalls",{"O"}},{"exchange_times_s",{60,120,600}}};
    const Json receiver={{"schema","hydrocouple.interval-transfer/1"},{"epoch_julian_day",2461041.5},
        {"species",Json::array({{{"id","B"},{"basis","kg"}},{{"id","A"},{"basis","kg"}}})},
        {"expected_transfers",3},{"receipt_file","native-receipts.csv"}};
    Json j={{"components",Json::array({
        {{"id","network"},{"info",{{"component_info_id","org.hydrocouple.openswmm.engine"}}},{"arguments",{{"configuration",configuration}}}},
        {{"id","receiver"},{"info",{{"component_info_id","org.hydrocouple.openswmm.test.sink"}}},{"arguments",{{"config",receiver}}}}
    })},{"connections",Json::array()},{"workflow",{{"strategy","time_stepped"}}}};
    for(const std::string field:{"transfer_id","epoch_julian_day","interval_start_s","interval_end_s","water_m3","enthalpy_j","species.A","species.B"})
        j["connections"].push_back({{"from",{{"component","network"},{"output",field}}},{"to",{{"component","receiver"},{"input",field}}}});
    save(j); CompositionDocument document;QString message;
    ASSERT_TRUE(document.load(directory.filePath("composition.json"),message))<<message.toStdString();
    SimulationManager manager(&registry);QSignalSpy finished(&manager,&SimulationManager::finished);
    ASSERT_TRUE(manager.start(document,message))<<message.toStdString();
    QElapsedTimer timer;timer.start();
    while(finished.empty() && timer.elapsed()<15000)QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    ASSERT_FALSE(finished.empty())<<manager.errors().join("; ").toStdString();
    ASSERT_TRUE(finished.first()[0].toBool())<<finished.first()[1].toString().toStdString()<<manager.errors().join("; ").toStdString();
    for(int i=0;i<3;++i) {
        std::ifstream file(directory.filePath(QString("native-results/accepted_%1.json").arg(i)).toStdString());
        ASSERT_TRUE(file.good());Json receipt;file>>receipt;
        EXPECT_EQ(receipt["transfer_id"],"network:"+std::to_string(i+1));
        for(const auto& remaining:receipt["unreceived_amounts"])EXPECT_EQ(remaining.get<double>(),0);
        EXPECT_GT(receipt["accepted_amounts"][0].get<double>(),0);
    }
    EXPECT_TRUE(QFile::exists(directory.filePath("native-results/model.out")));
    QFile file(directory.filePath("native-receipts.csv"));ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll().trimmed().split('\n').size(),4);
}
#endif
