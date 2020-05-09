//
// Copyright � 2020 Terry Moreland
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

#include "pch.h"

using std::copy;
using std::filesystem::path;
using std::initializer_list;
using std::move;
using std::optional;
using std::string;
using std::string_view;
using std::swap;
using std::unique_ptr;
using std::vector;
using std::wregex;

#define BOOST_TEST_MODULE SymbolPathService
#include <boost/test/included/unit_test.hpp>

#include <type_traits>
#include "TestAdapter.h"
#include "TestFixture.h"

using DebugSymbolManager::Model::Settings;
using DebugSymbolManager::Service::ISymbolPathService;
using DebugSymbolManager::Service::SymbolPathService;

#include "MockObjects.h"

#pragma warning(push)
#pragma warning(disable:4455)
using std::literals::string_literals::operator ""s;
#pragma warning(pop)

namespace {
    constexpr auto SYMBOL_PATH_VAR = "_NT_SYMBOL_PATH";
    constexpr auto SYMBOL_SERVER = "*SRV";
    
    struct ExpectedSetCall {

        Cardinality Cardinality;
        string Value;
        bool Success;

        void swap(ExpectedSetCall& other) noexcept {
            ::swap(Cardinality, other.Cardinality);
            ::swap(Value, other.Value);
            ::swap(Success, other.Success);
        }
    };

    void swap(ExpectedSetCall& left, ExpectedSetCall& right) noexcept {
        left.swap(right);
    }
    ExpectedSetCall SuccessfullySetTo(string value, optional<Cardinality> const& cardinality = nullopt) {
        return { cardinality.value_or(AnyNumber()), move(value), true };
    }

    struct Context {
        Context() {
            EnviromentRepository = make_unique<MockObjects::MockEnviromentRepository>();
            FileService = make_unique<MockObjects::MockFileService>();
        }
        Context(Context const&) = delete;
        Context(Context&& other) noexcept
            : EnviromentRepository(other.EnviromentRepository.release())
            , FileService(other.FileService.release())
            , Service(other.Service.release())
            , ApplicationPath(move(other.ApplicationPath))
            , InitialSymbolPath(move(other.InitialSymbolPath))
            , ExpectedSymbolPath(move(other.ExpectedSymbolPath))
            , NumberOfGetCalls(move(other.NumberOfGetCalls))
            , ExpectedSetCalls(move(other.ExpectedSetCalls)) 
            , ExistingDirectories(move(other.ExistingDirectories)) {
            other.Reset();
        }
        Context& operator=(Context const& other) = delete;
        Context& operator=(Context&& other) noexcept {
            if (this == &other)
                return *this;

            ::swap(EnviromentRepository, other.EnviromentRepository);
            ::swap(FileService, other.FileService);
            ::swap(ApplicationPath, other.ApplicationPath);
            ::swap(InitialSymbolPath, other.InitialSymbolPath);
            ::swap(ExpectedSymbolPath, other.ExpectedSymbolPath);
            ::swap(NumberOfGetCalls, other.NumberOfGetCalls);
            ::swap(ExpectedSetCalls, other.ExpectedSetCalls);
            ::swap(Service, other.Service);
            ::swap(ExistingDirectories, other.ExistingDirectories);

            other.Reset();
            return *this;
        }
        ~Context() = default;

        void Reset() noexcept {
            EnviromentRepository = make_unique<MockObjects::MockEnviromentRepository>();
            FileService = make_unique<MockObjects::MockFileService>();
            Settings = ::Settings{SYMBOL_SERVER};
            NumberOfGetCalls = AnyNumber();
            ExpectedSetCalls.clear();
            ExistingDirectories.clear();
        }

        unique_ptr<MockObjects::MockEnviromentRepository> EnviromentRepository{};
        unique_ptr<MockObjects::MockFileService> FileService{};
        unique_ptr<SymbolPathService> Service{};

        Settings Settings{SYMBOL_SERVER};
        std::string ApplicationPath{};
        std::string InitialSymbolPath{};
        std::string ExpectedSymbolPath{};
        Cardinality NumberOfGetCalls{AnyNumber()};
        vector<ExpectedSetCall> ExpectedSetCalls{};
        vector<string> ExistingDirectories{};
    };

    class ContextBuilder {
        Context m_context;

        template <typename UPDATER>
        ContextBuilder& UpdateObject(UPDATER updater) {
            updater(m_context);
            return *this;
        }
    public:
        static ContextBuilder ArrangeForConstructorTest(string const& initialVariableValue) {
            ContextBuilder builder;
            builder
                .WithInitialVariable(initialVariableValue)
                .WithGetCalledCountTimes(Exactly(1));
            return builder;
        }
        static ContextBuilder Arrange() {
            ContextBuilder builder;
            builder
                .WithInitialVariable(SYMBOL_SERVER);
            return builder;
        }

        Context&& Build() {
            if (!m_context.InitialSymbolPath.empty()) {
                EXPECT_CALL(*m_context.EnviromentRepository, GetVariable(SYMBOL_PATH_VAR))
                    .Times(m_context.NumberOfGetCalls)
                    .WillOnce(Return(optional(m_context.InitialSymbolPath)));
            }
            for (auto& expected : m_context.ExpectedSetCalls)
                EXPECT_CALL(*m_context.EnviromentRepository, SetVariable(SYMBOL_PATH_VAR, expected.Value))
                    .Times(expected.Cardinality)
                    .WillRepeatedly(Return(expected.Success));

            for (auto const& directory : m_context.ExistingDirectories)
                EXPECT_CALL(*m_context.FileService, DirectoryExists(string_view(directory)))
                    .WillRepeatedly(Return(true));

            return move(m_context);
        }

        ContextBuilder& WithInitialVariable(string symbolPath) {
            return UpdateObject(
                [&symbolPath](Context& context) {
                    context.InitialSymbolPath = move(symbolPath);
                });
        }
        ContextBuilder& WithExpectedVariable(string symbolPath) {
            return UpdateObject(
                [&symbolPath](Context& context) {
                    context.ExpectedSymbolPath = move(symbolPath);
                });
        }
        ContextBuilder& WithApplicationPath(string applicationPath) {
            return UpdateObject(
                [&applicationPath](auto& context) {
                    context.ApplicationPath = move(applicationPath);
                });
        }
        ContextBuilder& WithGetCalledCountTimes(Cardinality count) {
            return UpdateObject(
                [&count](auto& context) {
                    context.NumberOfGetCalls = move(count);
                });
        }
        template<typename TExpectedSetCall, typename... Args> // could be done with std::initializer_list but I wanted an excuse for varadic template
        ContextBuilder& WithExpectedSetCalls(TExpectedSetCall first, Args... expectedCalls) {
            // duck typing would prevent issues here but it's an excuse to use static assert and in this scenario ExpectedSetCall is the only valid type
            static_assert(std::is_same<TExpectedSetCall, ExpectedSetCall>::value, "invalid type, must use ExpectedSetCall");
            m_context.ExpectedSetCalls.push_back(move(first));
            return WithExpectedSetCalls(expectedCalls...);
        }
        template <typename TExpectedSetCall>
        ContextBuilder& WithExpectedSetCalls(TExpectedSetCall expectedCall) {
            m_context.ExpectedSetCalls.push_back(move(expectedCall));
            return *this;
        }
        ContextBuilder& WithExistingDirectories(std::initializer_list<string> directories) {
            return UpdateObject(
                [&directories](Context& context) {
                    copy(directories.begin(), directories.end(), 
                        back_inserter(context.ExistingDirectories));
                });
        }
        ContextBuilder& WithServiceCreated() {
            return UpdateObject(
                [](Context& context) {
                    auto service = make_unique<SymbolPathService>(context.Settings, *context.EnviromentRepository, *context.FileService);
                    context.Service  = move(service);
                });
        }
    };

}


BOOST_AUTO_TEST_CASE(ConstructorGetsCurrentSymbolPath) {
    // Arrange
    auto const context = ContextBuilder::ArrangeForConstructorTest("symPath123").Build();

    // Act
    SymbolPathService service(context.Settings, *context.EnviromentRepository, *context.FileService);
    
    // Assert -- handled by setup in arrange (EXPECT_CALL)
}


BOOST_AUTO_TEST_CASE(ConstructorUpdatesCurrentSymbolPathWhenHasValue) {
    // Arrange
    auto const context = ContextBuilder::ArrangeForConstructorTest("symPath123")
        .WithExpectedSetCalls(SuccessfullySetTo(SYMBOL_SERVER))
        .Build();

    // Act
    SymbolPathService service(context.Settings, *context.EnviromentRepository, *context.FileService);
    
    // Assert -- covered by expect call
}

BOOST_AUTO_TEST_CASE(UpdateApplicationPathChangesSymbolPath) {
    // Arrange
    auto const appPath = R"(C:\Program Files\Application)"s;
    auto const expectedVariableValue = string(SYMBOL_SERVER) + ";"s + appPath;
    auto context = ContextBuilder::Arrange()
        .WithExpectedSetCalls(SuccessfullySetTo(string(SYMBOL_SERVER) + ";"s + appPath, Exactly(1)))
        .WithExistingDirectories({appPath})
        .WithServiceCreated()
        .Build();

    // Act
    static_cast<void>(context.Service->UpdateApplicationPath(appPath));

    // Assert
    // again handled by the arrange and specifically expected set calls
}

BOOST_AUTO_TEST_CASE(UpdateApplicationPathReturnsSuccess) {
    // Arrange
    auto const appPath = R"(C:\Program Files\Application)"s;
    auto const expectedVariableValue = string(SYMBOL_SERVER) + ";"s + appPath;
    auto context = ContextBuilder::Arrange()
        .WithExpectedSetCalls(SuccessfullySetTo(string(SYMBOL_SERVER) + ";"s + appPath, Exactly(1)))
        .WithExistingDirectories({appPath})
        .WithServiceCreated()
        .Build();

    // Act
    auto const result = context.Service->UpdateApplicationPath(appPath);

    // Assert
    BOOST_ASSERT(result.IsSuccess());
}

BOOST_AUTO_TEST_CASE(UpdateReplacesOldApplicationPath) {
    // Arrange
    auto const appPath = R"(C:\Program Files\Application)"s;
    auto const replacementAppPath = R"(C:\Program Files (x86)\AlternateApplication)"s;
    auto const expectedVariableValue = string(SYMBOL_SERVER) + ";"s + appPath;
    auto const replacementExpectedVariableValue = string(SYMBOL_SERVER) + ";"s + replacementAppPath;

    auto context = ContextBuilder::Arrange()
        .WithExpectedSetCalls(SuccessfullySetTo(expectedVariableValue, Exactly(1)), SuccessfullySetTo(replacementExpectedVariableValue, Exactly(1)))
        .WithExistingDirectories({appPath, replacementAppPath})
        .WithServiceCreated()
        .Build();
    static_cast<void>(context.Service->UpdateApplicationPath(appPath));

    // Act
    static_cast<void>(context.Service->UpdateApplicationPath(replacementAppPath));

    // Assert
    // again handled by the arrange and specifically expected set calls
}

BOOST_AUTO_TEST_CASE(UpdateApplicaitonPathWithReplacementReturnsSuccess) {
    // Arrange
    auto const appPath = R"(C:\Program Files\Application)"s;
    auto const replacementAppPath = R"(C:\Program Files (x86)\AlternateApplication)"s;
    auto const expectedVariableValue = string(SYMBOL_SERVER) + ";"s + appPath;
    auto const replacementExpectedVariableValue = string(SYMBOL_SERVER) + ";"s + replacementAppPath;

    auto context = ContextBuilder::Arrange()
        .WithExpectedSetCalls(SuccessfullySetTo(expectedVariableValue, Exactly(1)), SuccessfullySetTo(replacementExpectedVariableValue, Exactly(1)))
        .WithExistingDirectories({appPath, replacementAppPath})
        .WithServiceCreated()
        .Build();
    static_cast<void>(context.Service->UpdateApplicationPath(appPath));

    // Act
    auto const result = context.Service->UpdateApplicationPath(replacementAppPath);

    // Assert
    BOOST_ASSERT(result.IsSuccess());
}
