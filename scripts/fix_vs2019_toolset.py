#!/usr/bin/env python3
"""
Fix Visual Studio 2019 project files for compatibility with VS2022
by updating platform toolset from v142 to v143
"""

import os
import re
import glob
from pathlib import Path

def fix_vcxproj_toolset(vcxproj_path):
    """Update a .vcxproj file to use v143 toolset instead of v142"""
    print(f"Processing: {vcxproj_path}")
    
    try:
        with open(vcxproj_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Count occurrences before replacement
        v142_count = content.count('<PlatformToolset>v142</PlatformToolset>')
        
        if v142_count == 0:
            print(f"  ⚪ No v142 toolset found")
            return False
        
        # Replace v142 with v143
        updated_content = content.replace(
            '<PlatformToolset>v142</PlatformToolset>',
            '<PlatformToolset>v143</PlatformToolset>'
        )
        
        # Verify the replacement worked
        v143_count = updated_content.count('<PlatformToolset>v143</PlatformToolset>')
        
        if v143_count != v142_count:
            print(f"  ❌ Replacement verification failed: expected {v142_count}, got {v143_count}")
            return False
        
        # Write the updated content back
        with open(vcxproj_path, 'w', encoding='utf-8') as f:
            f.write(updated_content)
        
        print(f"  ✅ Updated {v142_count} occurrences of v142 → v143")
        return True
        
    except Exception as e:
        print(f"  ❌ Error processing {vcxproj_path}: {e}")
        return False

def main():
    """Main function to fix all URG library VS2019 project files"""
    print("🔧 URG Library VS2019→VS2022 Toolset Compatibility Fix")
    print("=" * 60)
    
    # Base path for URG library VS2019 projects
    base_path = Path("external/urg_library/current/vs2019")
    
    if not base_path.exists():
        print(f"❌ URG library VS2019 directory not found: {base_path}")
        return 1
    
    # Find all .vcxproj files
    vcxproj_files = list(base_path.rglob("*.vcxproj"))
    
    if not vcxproj_files:
        print("❌ No .vcxproj files found")
        return 1
    
    print(f"Found {len(vcxproj_files)} Visual Studio project files:")
    for f in vcxproj_files:
        print(f"  - {f}")
    
    print("\nStarting toolset compatibility fix...")
    print("-" * 40)
    
    success_count = 0
    total_count = len(vcxproj_files)
    
    for vcxproj_file in vcxproj_files:
        if fix_vcxproj_toolset(vcxproj_file):
            success_count += 1
    
    print("-" * 40)
    print(f"🎯 Fix Results: {success_count}/{total_count} files successfully updated")
    
    if success_count == total_count:
        print("✅ All URG library project files updated to v143 toolset!")
        print("   VS2022 compatibility fix completed successfully.")
        return 0
    else:
        print(f"⚠️  {total_count - success_count} files had issues - manual review recommended")
        return 1

if __name__ == "__main__":
    exit(main())