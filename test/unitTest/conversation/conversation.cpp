/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>
#include <git2.h>
#include <filesystem>

#include "manager.h"
#include "jamidht/conversation.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"

using namespace std::string_literals;
using namespace DRing::Account;

namespace jami {
namespace test {

class ConversationTest : public CppUnit::TestFixture
{
public:
    ~ConversationTest() { DRing::fini(); }
    static std::string name() { return "Conversation"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string carlaId;

private:
    void testCreateConversation();
    void testGetConversation();
    void testGetConversationsAfterRm();
    void testRemoveInvalidConversation();
    void testRemoveConversationNoMember();
    void testRemoveConversationWithMember();
    void testAddMember();
    void testAddOfflineMemberThenConnects();
    void testGetMembers();
    void testSendMessage();
    void testSendMessageTriggerMessageReceived();
    void testMergeTwoDifferentHeads();
    void testGetRequests();
    void testDeclineRequest();
    void testSendMessageToMultipleParticipants();
    void testPingPongMessages();

    CPPUNIT_TEST_SUITE(ConversationTest);
    CPPUNIT_TEST(testCreateConversation);
    CPPUNIT_TEST(testGetConversation);
    CPPUNIT_TEST(testGetConversationsAfterRm);
    CPPUNIT_TEST(testRemoveInvalidConversation);
    CPPUNIT_TEST(testRemoveConversationNoMember);
    CPPUNIT_TEST(testRemoveConversationWithMember);
    CPPUNIT_TEST(testAddMember);
    CPPUNIT_TEST(testAddOfflineMemberThenConnects);
    CPPUNIT_TEST(testGetMembers);
    CPPUNIT_TEST(testSendMessage);
    CPPUNIT_TEST(testSendMessageTriggerMessageReceived);
    CPPUNIT_TEST(testGetRequests);
    CPPUNIT_TEST(testDeclineRequest);
    CPPUNIT_TEST(testSendMessageToMultipleParticipants);
    CPPUNIT_TEST(testPingPongMessages);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationTest, ConversationTest::name());

void
ConversationTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "CARLA";
    details[ConfProperties::ALIAS] = "CARLA";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    carlaId = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    Manager::instance().sendRegister(carlaId, false);
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                bool ready = false;
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready = (daemonStatus == "REGISTERED");
                details = bobAccount->getVolatileAccountDetails();
                daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready &= (daemonStatus == "REGISTERED");
            }));
    DRing::registerSignalHandlers(confHandlers);
    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::tearDown()
{
    auto currentAccSize = Manager::instance().getAccountList().size();
    Manager::instance().removeAccount(aliceId, true);
    Manager::instance().removeAccount(bobId, true);
    Manager::instance().removeAccount(carlaId, true);
    // Because cppunit is not linked with dbus, just poll if removed
    for (int i = 0; i < 40; ++i) {
        if (Manager::instance().getAccountList().size() <= currentAccSize - 3)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void
ConversationTest::testCreateConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = aliceAccount->startConversation();
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; });
    CPPUNIT_ASSERT(conversationReady);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto adminCrt = repoPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(adminCrt));
    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());
    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);
    CPPUNIT_ASSERT(adminCrtStr == parentCert);
    auto deviceCrt = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + aliceDeviceId
                     + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(deviceCrt));
    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)),
                             std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationTest::testGetConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();
    auto convId = aliceAccount->startConversation();

    auto conversations = aliceAccount->getConversations();
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(conversations.front() == convId);
}

void
ConversationTest::testGetConversationsAfterRm()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = aliceAccount->startConversation();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    auto conversations = aliceAccount->getConversations();
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(aliceAccount->removeConversation(convId));
    conversations = aliceAccount->getConversations();
    CPPUNIT_ASSERT(conversations.size() == 0);
}

void
ConversationTest::testRemoveInvalidConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = aliceAccount->startConversation();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    auto conversations = aliceAccount->getConversations();
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(!aliceAccount->removeConversation("foo"));
    conversations = aliceAccount->getConversations();
    CPPUNIT_ASSERT(conversations.size() == 1);
}

void
ConversationTest::testRemoveConversationNoMember()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = aliceAccount->startConversation();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto conversations = aliceAccount->getConversations();
    CPPUNIT_ASSERT(conversations.size() == 1);
    // Removing the conversation will erase all related files
    CPPUNIT_ASSERT(aliceAccount->removeConversation(convId));
    conversations = aliceAccount->getConversations();
    CPPUNIT_ASSERT(conversations.size() == 0);
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));
}

void
ConversationTest::testRemoveConversationWithMember()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         bobSeeAliceRemoved = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            } else if (accountId == bobId && conversationId == convId
                       && message["type"] == "member") {
                bobSeeAliceRemoved = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));
    CPPUNIT_ASSERT(memberMessageGenerated);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvitedFile = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri
                          + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(bobInvitedFile));

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    bobInvitedFile = clonedPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri
                     + ".crt";
    CPPUNIT_ASSERT(!fileutils::isFile(bobInvitedFile));
    // Remove conversation from alice once member confirmed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));

    bobSeeAliceRemoved = false;
    aliceAccount->removeConversation(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return bobSeeAliceRemoved; }));
    std::this_thread::sleep_for(std::chrono::seconds(3));
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));
}

void
ConversationTest::testAddMember()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = aliceAccount->startConversation();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));
    CPPUNIT_ASSERT(memberMessageGenerated);
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri
                      + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    bobInvited = clonedPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri + ".crt";
    CPPUNIT_ASSERT(!fileutils::isFile(bobInvited));
}

void
ConversationTest::testAddOfflineMemberThenConnects()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == carlaId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, carlaUri));
    Manager::instance().sendRegister(carlaId, true);
    cv.wait_for(lk, std::chrono::seconds(60));
    CPPUNIT_ASSERT(requestReceived);

    carlaAccount->acceptConversationRequest(convId);
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(conversationReady);
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
}

void
ConversationTest::testGetMembers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageReceived = false;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == aliceId) {
                messageReceived = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    // Start a conversation and add member
    auto convId = aliceAccount->startConversation();

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto members = aliceAccount->getConversationMembers(convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "invited");

    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; });
    messageReceived = false;
    bobAccount->acceptConversationRequest(convId);
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; });
    members = bobAccount->getConversationMembers(convId);
    CPPUNIT_ASSERT(members.size() == 2);
    cv.wait_for(lk, std::chrono::seconds(60), [&]() { return messageReceived; });
    members = aliceAccount->getConversationMembers(convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "member");
}

void
ConversationTest::testSendMessage()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            } else {
                messageAliceReceived += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = aliceAccount->startConversation();

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(requestReceived);

    bobAccount->acceptConversationRequest(convId);
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(conversationReady);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageAliceReceived == 1; });

    aliceAccount->sendMessage(convId, "hi");
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageBobReceived == 1; });
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testSendMessageTriggerMessageReceived()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageReceived = 0;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            messageReceived += 1;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& /* accountId */, const std::string& /* conversationId */) {
            conversationReady = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = aliceAccount->startConversation();
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(conversationReady);

    aliceAccount->sendMessage(convId, "hi");
    cv.wait_for(lk, std::chrono::seconds(30), [&] { return messageReceived == 1; });
    CPPUNIT_ASSERT(messageReceived == 1);
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testMergeTwoDifferentHeads()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, carlaGotMessage = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == carlaId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == carlaId && conversationId == convId) {
                carlaGotMessage = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, carlaUri, false));

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + aliceAccount->getAccountID() + DIR_SEPARATOR_STR + "conversations";
    auto repoPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + carlaAccount->getAccountID() + DIR_SEPARATOR_STR + "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                       + DIR_SEPARATOR_STR + "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaAccount->getAccountID()
                       + DIR_SEPARATOR_STR + "convInfo";
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Accept for alice and makes different heads
    ConversationRepository repo(carlaAccount, convId);
    repo.join();

    aliceAccount->sendMessage(convId, "hi");
    aliceAccount->sendMessage(convId, "sup");
    aliceAccount->sendMessage(convId, "jami");

    // Start Carla, should merge and all messages should be there
    Manager::instance().sendRegister(carlaId, true);
    carlaGotMessage = false;
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return carlaGotMessage; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testGetRequests()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = aliceAccount->startConversation();

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(requestReceived);

    auto requests = bobAccount->getConversationRequests();
    CPPUNIT_ASSERT(requests.size() == 1);
    CPPUNIT_ASSERT(requests.front()["id"] == convId);
}

void
ConversationTest::testDeclineRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = aliceAccount->startConversation();

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(requestReceived);

    bobAccount->declineConversationRequest(convId);
    // Decline request
    auto requests = bobAccount->getConversationRequests();
    CPPUNIT_ASSERT(requests.size() == 0);
}

void
ConversationTest::testSendMessageToMultipleParticipants()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    // Enable carla
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    cv.wait_for(lk, std::chrono::seconds(30));
    confHandlers.clear();
    DRing::unregisterSignalHandlers();

    auto messageReceivedAlice = 0;
    auto messageReceivedBob = 0;
    auto messageReceivedCarla = 0;
    auto requestReceived = 0;
    auto conversationReady = 0;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == aliceId)
                messageReceivedAlice += 1;
            if (accountId == bobId)
                messageReceivedBob += 1;
            if (accountId == carlaId)
                messageReceivedCarla += 1;
            cv.notify_one();
        }));

    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived += 1;
                cv.notify_one();
            }));

    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& /*accountId*/, const std::string& /* conversationId */) {
            conversationReady += 1;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = aliceAccount->startConversation();

    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));
    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, carlaUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(60), [&]() { return requestReceived == 2; }));

    messageReceivedAlice = 0;
    bobAccount->acceptConversationRequest(convId);
    carlaAccount->acceptConversationRequest(convId);
    // >= because we can have merges cause the accept commits
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return conversationReady == 3 && messageReceivedAlice >= 2;
    }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaAccount->getAccountID()
               + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    aliceAccount->sendMessage(convId, "hi");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return messageReceivedBob >= 1 && messageReceivedCarla >= 1;
    }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testPingPongMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            }
            if (accountId == aliceId) {
                messageAliceReceived += 1;
            }
            if (messageAliceReceived == messageBobReceived)
                cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    auto convId = aliceAccount->startConversation();
    CPPUNIT_ASSERT(aliceAccount->addConversationMember(convId, bobUri));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(requestReceived);
    messageAliceReceived = 0;
    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return conversationReady && messageAliceReceived == 1;
    }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    messageBobReceived = 0;
    messageAliceReceived = 0;
    aliceAccount->sendMessage(convId, "ping");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 1 && messageAliceReceived == 1;
    }));
    bobAccount->sendMessage(convId, "pong");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 2 && messageAliceReceived == 2;
    }));
    bobAccount->sendMessage(convId, "ping");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 3 && messageAliceReceived == 3;
    }));
    aliceAccount->sendMessage(convId, "pong");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 4 && messageAliceReceived == 4;
    }));
    DRing::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationTest::name())