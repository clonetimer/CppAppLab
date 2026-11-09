#include "tcp_chat.hpp"

#include <gtest/gtest.h>

TEST(TcpChatLearnerTest, BindsAnEphemeralPort) {
    FAIL() << "TODO(W7-CHAT-TEST-1): construct a server on port zero and inspect its port.";
}
TEST(TcpChatLearnerTest, RejectsZeroBacklog) {
    FAIL() << "TODO(W7-CHAT-TEST-2): verify invalid backlog is rejected.";
}
TEST(TcpChatLearnerTest, RejectsZeroClientLimit) {
    FAIL() << "TODO(W7-CHAT-TEST-3): verify max_clients must be positive.";
}
TEST(TcpChatLearnerTest, RegistersNickname) {
    FAIL() << "TODO(W7-CHAT-TEST-4): connect a client and register a nickname.";
}
TEST(TcpChatLearnerTest, RejectsDuplicateNickname) {
    FAIL() << "TODO(W7-CHAT-TEST-5): ensure connected nicknames are unique.";
}
TEST(TcpChatLearnerTest, BroadcastsMessagesToOtherClients) {
    FAIL() << "TODO(W7-CHAT-TEST-6): connect two clients and verify message delivery.";
}
TEST(TcpChatLearnerTest, HandlesFragmentedCommands) {
    FAIL() << "TODO(W7-CHAT-TEST-7): split a newline-delimited command across writes.";
}
TEST(TcpChatLearnerTest, RejectsMessageBeforeRegistration) {
    FAIL() << "TODO(W7-CHAT-TEST-8): send MSG before NICK and inspect the response.";
}
TEST(TcpChatLearnerTest, DisconnectsOnQuit) {
    FAIL() << "TODO(W7-CHAT-TEST-9): issue QUIT and verify connection cleanup.";
}
TEST(TcpChatLearnerTest, StopRequestWakesEventLoop) {
    FAIL() << "TODO(W7-CHAT-TEST-10): run on another thread and request a prompt stop.";
}
