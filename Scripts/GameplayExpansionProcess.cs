using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32.SafeHandles;

namespace AuraGameplayExpansion
{
    // A suspended child is assigned before it can create descendants. Cleanup
    // targets this kernel job, never names or a reconstructed PID ancestry tree.
    public sealed class OwnedChild : IDisposable
    {
        private IntPtr job;
        private StreamReader stdout;
        private StreamReader stderr;
        public Process Process { get; private set; }
        public Task<string> StandardOutput { get; private set; }
        public Task<string> StandardError { get; private set; }

        public static OwnedChild Start(string executable, string[] arguments, string workingDirectory)
        {
            var child = new OwnedChild();
            IntPtr outRead = IntPtr.Zero, outWrite = IntPtr.Zero;
            IntPtr errRead = IntPtr.Zero, errWrite = IntPtr.Zero, input = IntPtr.Zero;
            var processInfo = new PROCESS_INFORMATION();
            try
            {
                child.job = CreateJobObject(IntPtr.Zero, null);
                Check(child.job != IntPtr.Zero, "CreateJobObject");
                var limits = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
                limits.BasicLimitInformation.LimitFlags = 0x2000; // KILL_ON_JOB_CLOSE; no breakaway.
                Check(SetInformationJobObject(child.job, 9, ref limits,
                    (uint)Marshal.SizeOf<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>()), "SetInformationJobObject");

                var security = new SECURITY_ATTRIBUTES
                {
                    nLength = Marshal.SizeOf<SECURITY_ATTRIBUTES>(),
                    bInheritHandle = true
                };
                Check(CreatePipe(out outRead, out outWrite, ref security, 0), "CreatePipe(stdout)");
                Check(SetHandleInformation(outRead, 1, 0), "SetHandleInformation(stdout)");
                Check(CreatePipe(out errRead, out errWrite, ref security, 0), "CreatePipe(stderr)");
                Check(SetHandleInformation(errRead, 1, 0), "SetHandleInformation(stderr)");
                input = CreateFile("NUL", 0x80000000, 3, ref security, 3, 0, IntPtr.Zero);
                Check(input != new IntPtr(-1), "CreateFile(NUL)");
                var startup = new STARTUPINFO
                {
                    cb = Marshal.SizeOf<STARTUPINFO>(),
                    dwFlags = 0x100 | 1, // USESTDHANDLES and USESHOWWINDOW.
                    wShowWindow = 0,
                    hStdInput = input,
                    hStdOutput = outWrite,
                    hStdError = errWrite
                };
                var command = new StringBuilder(Quote(executable));
                foreach (string argument in arguments) command.Append(' ').Append(Quote(argument));
                Check(CreateProcess(executable, command, IntPtr.Zero, IntPtr.Zero, true,
                    0x08000000 | 4, IntPtr.Zero, workingDirectory, ref startup, out processInfo),
                    "CreateProcess(suspended)");
                Check(AssignProcessToJobObject(child.job, processInfo.hProcess), "AssignProcessToJobObject");

                // Hold the actual process handle while it is still suspended;
                // later PID reuse cannot redirect this Process object.
                child.Process = Process.GetProcessById((int)processInfo.dwProcessId);
                IntPtr heldProcessHandle = child.Process.Handle;
                child.stdout = new StreamReader(new FileStream(new SafeFileHandle(outRead, true), FileAccess.Read));
                outRead = IntPtr.Zero;
                child.stderr = new StreamReader(new FileStream(new SafeFileHandle(errRead, true), FileAccess.Read));
                errRead = IntPtr.Zero;
                Close(ref outWrite);
                Close(ref errWrite);
                Close(ref input);
                child.StandardOutput = child.stdout.ReadToEndAsync();
                child.StandardError = child.stderr.ReadToEndAsync();
                Check(ResumeThread(processInfo.hThread) != uint.MaxValue, "ResumeThread");
                return child;
            }
            catch
            {
                // The native handle belongs only to the process we created.
                // It may still be suspended if job assignment failed.
                if (processInfo.hProcess != IntPtr.Zero) TerminateProcess(processInfo.hProcess, 1);
                child.Dispose();
                throw;
            }
            finally
            {
                Close(ref processInfo.hThread);
                Close(ref processInfo.hProcess);
                Close(ref outRead);
                Close(ref outWrite);
                Close(ref errRead);
                Close(ref errWrite);
                Close(ref input);
            }
        }

        public int ActiveProcessCount { get { return (int)Accounting().ActiveProcesses; } }
        public int TotalProcessCount { get { return (int)Accounting().TotalProcesses; } }

        public int[] ActiveProcessIds
        {
            get
            {
                for (int capacity = 64; capacity <= 65536; capacity *= 2)
                {
                    int size = 8 + capacity * IntPtr.Size;
                    IntPtr buffer = Marshal.AllocHGlobal(size);
                    try
                    {
                        uint returned;
                        if (QueryInformationJobObject(job, 3, buffer, (uint)size, out returned))
                        {
                            int count = Marshal.ReadInt32(buffer, 4);
                            if (count < 0 || count > capacity) throw new InvalidOperationException("Invalid job member count");
                            var ids = new int[count];
                            for (int i = 0; i < count; i++) ids[i] = checked((int)Marshal.ReadIntPtr(buffer, 8 + i * IntPtr.Size).ToInt64());
                            return ids;
                        }
                        int error = Marshal.GetLastWin32Error();
                        if (error != 234) throw new Win32Exception(error, "QueryInformationJobObject(process ids)");
                    }
                    finally { Marshal.FreeHGlobal(buffer); }
                }
                throw new InvalidOperationException("Job member reporting limit exceeded");
            }
        }

        public void Terminate() { Check(TerminateJobObject(job, 1), "TerminateJobObject"); }

        private JOBOBJECT_BASIC_ACCOUNTING_INFORMATION Accounting()
        {
            int size = Marshal.SizeOf<JOBOBJECT_BASIC_ACCOUNTING_INFORMATION>();
            IntPtr buffer = Marshal.AllocHGlobal(size);
            try
            {
                uint returned;
                Check(QueryInformationJobObject(job, 1, buffer, (uint)size, out returned), "QueryInformationJobObject(accounting)");
                return Marshal.PtrToStructure<JOBOBJECT_BASIC_ACCOUNTING_INFORMATION>(buffer);
            }
            finally { Marshal.FreeHGlobal(buffer); }
        }

        public void Dispose()
        {
            // Closing this non-inheritable handle kills only members of this job,
            // including descendants missed between progress snapshots.
            Close(ref job);
            if (Process != null) { Process.Dispose(); Process = null; }
            if (stdout != null) { stdout.Dispose(); stdout = null; }
            if (stderr != null) { stderr.Dispose(); stderr = null; }
        }

        private static string Quote(string value)
        {
            if (value == null) throw new ArgumentNullException(nameof(value));
            var result = new StringBuilder("\"");
            int slashes = 0;
            foreach (char current in value)
            {
                if (current == '\\') { slashes++; continue; }
                if (current == '"') result.Append('\\', slashes * 2 + 1).Append('"');
                else result.Append('\\', slashes).Append(current);
                slashes = 0;
            }
            return result.Append('\\', slashes * 2).Append('"').ToString();
        }

        private static void Check(bool success, string operation)
        {
            if (!success) throw new Win32Exception(Marshal.GetLastWin32Error(), operation);
        }
        private static void Close(ref IntPtr handle)
        {
            if (handle != IntPtr.Zero && handle != new IntPtr(-1)) CloseHandle(handle);
            handle = IntPtr.Zero;
        }

        [StructLayout(LayoutKind.Sequential)] private struct SECURITY_ATTRIBUTES
        { public int nLength; public IntPtr lpSecurityDescriptor; [MarshalAs(UnmanagedType.Bool)] public bool bInheritHandle; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] private struct STARTUPINFO
        {
            public int cb; public string lpReserved, lpDesktop, lpTitle;
            public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
            public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
        }
        [StructLayout(LayoutKind.Sequential)] private struct PROCESS_INFORMATION
        { public IntPtr hProcess, hThread; public uint dwProcessId, dwThreadId; }
        [StructLayout(LayoutKind.Sequential)] private struct JOBOBJECT_BASIC_LIMIT_INFORMATION
        {
            public long PerProcessUserTimeLimit, PerJobUserTimeLimit; public uint LimitFlags;
            public UIntPtr MinimumWorkingSetSize, MaximumWorkingSetSize; public uint ActiveProcessLimit;
            public UIntPtr Affinity; public uint PriorityClass, SchedulingClass;
        }
        [StructLayout(LayoutKind.Sequential)] private struct IO_COUNTERS
        { public ulong ReadOperationCount, WriteOperationCount, OtherOperationCount, ReadTransferCount, WriteTransferCount, OtherTransferCount; }
        [StructLayout(LayoutKind.Sequential)] private struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
        {
            public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation; public IO_COUNTERS IoInfo;
            public UIntPtr ProcessMemoryLimit, JobMemoryLimit, PeakProcessMemoryUsed, PeakJobMemoryUsed;
        }
        [StructLayout(LayoutKind.Sequential)] private struct JOBOBJECT_BASIC_ACCOUNTING_INFORMATION
        {
            public long TotalUserTime, TotalKernelTime, ThisPeriodTotalUserTime, ThisPeriodTotalKernelTime;
            public uint TotalPageFaultCount, TotalProcesses, ActiveProcesses, TotalTerminatedProcesses;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] private static extern IntPtr CreateJobObject(IntPtr security, string name);
        [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool SetInformationJobObject(IntPtr job, int type, ref JOBOBJECT_EXTENDED_LIMIT_INFORMATION value, uint size);
        [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool QueryInformationJobObject(IntPtr job, int type, IntPtr value, uint size, out uint returned);
        [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
        [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool TerminateJobObject(IntPtr job, uint exitCode);
        [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool CreatePipe(out IntPtr read, out IntPtr write, ref SECURITY_ATTRIBUTES security, uint size);
        [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool SetHandleInformation(IntPtr handle, uint mask, uint flags);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] private static extern IntPtr CreateFile(string name, uint access, uint share, ref SECURITY_ATTRIBUTES security, uint disposition, uint flags, IntPtr template);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool CreateProcess(string application, StringBuilder command, IntPtr processAttributes, IntPtr threadAttributes, [MarshalAs(UnmanagedType.Bool)] bool inherit, uint flags, IntPtr environment, string directory, ref STARTUPINFO startup, out PROCESS_INFORMATION process);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern uint ResumeThread(IntPtr thread);
        [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool TerminateProcess(IntPtr process, uint exitCode);
        [DllImport("kernel32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool CloseHandle(IntPtr handle);
    }
}
