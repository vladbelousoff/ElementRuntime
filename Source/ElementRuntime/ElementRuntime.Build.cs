using System;
using System.IO;
using UnrealBuildTool;

public class ElementRuntime : ModuleRules
{
    public ElementRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        bUseRTTI = false;
        bEnableExceptions = false;

        PublicDefinitions.Add("OAK_ATOMIC_REFCOUNT=1");
        PublicDefinitions.Add("OAK_STATIC=1");

        var elementRoot = Environment.GetEnvironmentVariable("ELEMENT_ROOT");
        if (String.IsNullOrWhiteSpace(elementRoot))
        {
            elementRoot = Path.Combine(PluginDirectory, "element");
        }

        var elmInclude = Path.Combine(elementRoot, "include");
        if (!Directory.Exists(elmInclude))
        {
            throw new BuildException("Element include directory not found: " + elmInclude + ". Set ELEMENT_ROOT to an Element checkout or install root.");
        }
        PublicIncludePaths.Add(elmInclude);

        var bridgeRoot = Environment.GetEnvironmentVariable("ELEMENT_OAK_BRIDGE_ROOT");
        if (String.IsNullOrWhiteSpace(bridgeRoot))
        {
            bridgeRoot = Path.Combine(PluginDirectory, "element-oak-bridge");
        }

        var bridgeInclude = Path.Combine(bridgeRoot, "include");
        if (!Directory.Exists(bridgeInclude))
        {
            throw new BuildException("Element Oak Bridge include directory not found: " + bridgeInclude + ". Set ELEMENT_OAK_BRIDGE_ROOT to an element-oak-bridge checkout or install root.");
        }
        PrivateIncludePaths.Add(bridgeInclude);

        var oakRoot = Environment.GetEnvironmentVariable("OAK_ROOT");
        if (String.IsNullOrWhiteSpace(oakRoot))
        {
            oakRoot = Path.Combine(PluginDirectory, "oak");
        }

        PublicIncludePaths.Add(Path.Combine(oakRoot, "include"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "runtime"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "compiler"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "compiler", "internal"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "common"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "lexer"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "parser"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "vm"));
        PrivateIncludePaths.Add(Path.Combine(oakRoot, "src", "stdlib"));

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "CoreUObject",
            "Engine",
            "InputCore"
        });
    }
}
