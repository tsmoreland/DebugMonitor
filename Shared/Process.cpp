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
#include "Process.h"
#include "ProcessImpl.h"

using ProcessImpl = Shared::Infrastructure::ProcessImpl;

using std::filesystem::path;
using std::make_unique;
using std::move;
using std::nullopt;
using std::optional;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;

namespace Shared::Model
{
    unique_ptr<IProcess> Process::Start(string_view const& filename, string_view const& arguments)
    {
        unique_ptr<Process> process{};
        process.reset(new Process(ProcessImpl::Start(filename, arguments).release()));
        return process;
    }
    vector<unique_ptr<IProcess>> Process::GetProcessesByName(std::string_view const& processName)
    {
        const size_t minimumSize = 100;
        auto processImplementations = ProcessImpl::GetProcessesByName(processName);
        vector<unique_ptr<IProcess>> processes{};
        processes.reserve(minimumSize);

        for (auto& pImpl : processImplementations)
            processes.emplace_back(new Process(pImpl.release()));
        return processes;
    }

    Process::Process()
    {
        _pImpl = make_unique<ProcessImpl>();
    }

    Process::~Process() 
    {
        if (IsRunning())
            WaitForExit();
    }

    unsigned long Process::GetId() const noexcept
    {
        return _pImpl->GetId();
    }

    bool Process::IsRunning() const noexcept
    {
        return _pImpl->IsRunning();
    }

    optional<DWORD> Process::ExitCode() const noexcept
    {
        return _pImpl->ExitCode();
    }

    void Process::WaitForExit() const noexcept
    {
        _pImpl-> WaitForExit();
    }

    optional<path> Process::GetPathToRunningProcess(string_view const& processName) const noexcept
    {
        try
        {
            return ProcessImpl::GetPathToRunningProcess(processName);
        }
        catch (std::exception&)
        {
            return nullopt;
        }
    }

    Process::Process(ProcessImpl* pImpl)
        : IProcess()
        , _pImpl{pImpl}
    {
    }
    Process& Process::operator=(Process&& other) noexcept
    {
        _pImpl = move(other._pImpl);
        return *this;
    }


}