# Homebrew formula for json-gen-c
# Install via tap: brew install zltl/tap/json-gen-c
#
# To set up the tap repository, create a GitHub repo named
# "homebrew-tap" under the zltl org/user with this file at
# Formula/json-gen-c.rb

class JsonGenC < Formula
  desc "Schema-first JSON code generator for C"
  homepage "https://github.com/zltl/json-gen-c"
  url "https://github.com/zltl/json-gen-c/archive/refs/tags/v0.9.0.tar.gz"
  # TODO: Update sha256 after creating the v0.9.0 release tag
  sha256 "PLACEHOLDER_SHA256"
  license "GPL-3.0-or-later"
  head "https://github.com/zltl/json-gen-c.git", branch: "main"

  depends_on "xxd" => :build

  def install
    system "make", "-j#{ENV.make_jobs}"
    system "make", "install", "PREFIX=#{prefix}", "DESTDIR="
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/json-gen-c --version")

    # Verify code generation works
    (testpath/"test.json-gen-c").write <<~SCHEMA
      struct Point {
        int x;
        int y;
      }
    SCHEMA
    system bin/"json-gen-c", "-s", testpath/"test.json-gen-c",
                             "-o", testpath/"gen"
    assert_predicate testpath/"gen/json.gen.h", :exist?
    assert_predicate testpath/"gen/json.gen.c", :exist?
  end
end
