package main

import (
	"encoding/json"
	"go_test/gen"
	"math"
	"testing"
)

func TestBasicStructRoundtrip(t *testing.T) {
	p := gen.Person{Name: "Alice", Age: "30"}
	data, err := json.Marshal(p)
	if err != nil {
		t.Fatal(err)
	}
	var p2 gen.Person
	if err := json.Unmarshal(data, &p2); err != nil {
		t.Fatal(err)
	}
	if p2.Name != "Alice" || p2.Age != "30" {
		t.Fatalf("roundtrip mismatch: got %+v", p2)
	}
}

func TestEnumRoundtrip(t *testing.T) {
	type wrapper struct {
		C gen.Color `json:"color"`
	}
	w := wrapper{C: gen.ColorBLUE}
	data, err := json.Marshal(w)
	if err != nil {
		t.Fatal(err)
	}
	var w2 wrapper
	if err := json.Unmarshal(data, &w2); err != nil {
		t.Fatal(err)
	}
	if w2.C != gen.ColorBLUE {
		t.Fatalf("expected BLUE, got %v", w2.C)
	}
}

func TestOptionalFields(t *testing.T) {
	o := gen.OptionalOnlyStruct{Id: 1}
	data, err := json.Marshal(o)
	if err != nil {
		t.Fatal(err)
	}
	s := string(data)
	// Optional fields with omitempty should not appear when nil
	if containsKey(s, "name") || containsKey(s, "score") {
		t.Fatalf("nil optional fields should be omitted: %s", s)
	}
	// Now set some optional fields
	name := "test"
	score := int32(100)
	o.Name = &name
	o.Score = &score
	data, err = json.Marshal(o)
	if err != nil {
		t.Fatal(err)
	}
	var o2 gen.OptionalOnlyStruct
	if err := json.Unmarshal(data, &o2); err != nil {
		t.Fatal(err)
	}
	if *o2.Name != "test" || *o2.Score != 100 {
		t.Fatalf("optional roundtrip mismatch: got %+v", o2)
	}
}

func TestNullableFields(t *testing.T) {
	n := gen.NullableOnlyStruct{Id: 1}
	data, err := json.Marshal(n)
	if err != nil {
		t.Fatal(err)
	}
	// Nullable fields should appear as null (no omitempty)
	s := string(data)
	if !containsKey(s, "name") {
		t.Fatalf("nullable fields should appear even when nil: %s", s)
	}
	var n2 gen.NullableOnlyStruct
	if err := json.Unmarshal(data, &n2); err != nil {
		t.Fatal(err)
	}
	if n2.Name != nil {
		t.Fatalf("expected nil name, got %v", n2.Name)
	}
}

func TestJsonAlias(t *testing.T) {
	a := gen.AliasBasic{Username: "alice", Created: 1234567890, Id: 42}
	data, err := json.Marshal(a)
	if err != nil {
		t.Fatal(err)
	}
	s := string(data)
	// Check the JSON key names are aliased
	if !containsKey(s, "user_name") || !containsKey(s, "created_at") {
		t.Fatalf("expected aliased keys: %s", s)
	}
	var a2 gen.AliasBasic
	if err := json.Unmarshal(data, &a2); err != nil {
		t.Fatal(err)
	}
	if a2.Username != "alice" || a2.Id != 42 {
		t.Fatalf("alias roundtrip mismatch: got %+v", a2)
	}
}

func TestNestedStruct(t *testing.T) {
	n := gen.NestedStruct{
		Id:   1,
		Name: "test",
		Embedded: gen.TestStruct{
			Int_val:    42,
			Bool_val:   true,
			Sstr_val:   "hello",
			Double_val: 3.14,
		},
	}
	data, err := json.Marshal(n)
	if err != nil {
		t.Fatal(err)
	}
	var n2 gen.NestedStruct
	if err := json.Unmarshal(data, &n2); err != nil {
		t.Fatal(err)
	}
	if n2.Embedded.Int_val != 42 || n2.Embedded.Sstr_val != "hello" {
		t.Fatalf("nested roundtrip mismatch: got %+v", n2)
	}
}

func TestDynamicArrays(t *testing.T) {
	a := gen.ComplexStruct{
		Int_array:    []int32{1, 2, 3},
		String_array: []string{"a", "b"},
		Simple_int:   1,
	}
	data, err := json.Marshal(a)
	if err != nil {
		t.Fatal(err)
	}
	var a2 gen.ComplexStruct
	if err := json.Unmarshal(data, &a2); err != nil {
		t.Fatal(err)
	}
	if len(a2.Int_array) != 3 || a2.Int_array[0] != 1 || a2.Int_array[2] != 3 {
		t.Fatalf("dynamic array roundtrip mismatch: got %+v", a2)
	}
}

func TestFixedArrays(t *testing.T) {
	f := gen.FixedArrayStruct{
		Fixed_ints:    [5]int32{1, 2, 3, 4, 5},
		Fixed_longs:   [3]int64{100, 200, 300},
		Fixed_strings: [3]string{"a", "b", "c"},
		Fixed_bools:   [2]bool{true, false},
		Fixed_colors:  [3]gen.Color{gen.ColorRED, gen.ColorGREEN, gen.ColorBLUE},
	}
	data, err := json.Marshal(f)
	if err != nil {
		t.Fatal(err)
	}
	var f2 gen.FixedArrayStruct
	if err := json.Unmarshal(data, &f2); err != nil {
		t.Fatal(err)
	}
	if f2.Fixed_ints != [5]int32{1, 2, 3, 4, 5} {
		t.Fatalf("fixed array mismatch: got %v", f2.Fixed_ints)
	}
}

func TestMapRoundtrip(t *testing.T) {
	m := gen.MapIntStruct{
		Scores: map[string]int32{"alice": 100, "bob": 90},
	}
	data, err := json.Marshal(m)
	if err != nil {
		t.Fatal(err)
	}
	var m2 gen.MapIntStruct
	if err := json.Unmarshal(data, &m2); err != nil {
		t.Fatal(err)
	}
	if m2.Scores["alice"] != 100 || m2.Scores["bob"] != 90 {
		t.Fatalf("map roundtrip mismatch: got %+v", m2)
	}
}

func TestDefaultValues(t *testing.T) {
	d := gen.NewDefaultBasic()
	if d.Count != 42 || d.Big != 1000000 || d.Active != true || d.Label != "hello" {
		t.Fatalf("default values mismatch: got %+v", d)
	}
	if math.Abs(float64(d.Ratio)-3.14) > 0.01 {
		t.Fatalf("default ratio mismatch: got %v", d.Ratio)
	}
}

func TestDefaultPrecise(t *testing.T) {
	d := gen.NewDefaultPrecise()
	if d.I8 != -1 || d.I16 != 256 || d.I32 != -100000 {
		t.Fatalf("precise int defaults mismatch: got %+v", d)
	}
	if d.U8 != 255 || d.U16 != 65535 || d.U32 != 42 {
		t.Fatalf("precise uint defaults mismatch: got %+v", d)
	}
}

func TestOneofRoundtrip(t *testing.T) {
	s := gen.Shape{
		Tag:    "circle",
		Circle: &gen.Circle{Radius: 5.0},
	}
	data, err := json.Marshal(s)
	if err != nil {
		t.Fatal(err)
	}
	var s2 gen.Shape
	if err := json.Unmarshal(data, &s2); err != nil {
		t.Fatal(err)
	}
	if s2.Tag != "circle" || s2.Circle == nil || s2.Circle.Radius != 5.0 {
		t.Fatalf("oneof roundtrip mismatch: got %+v", s2)
	}
}

func TestOneofRectangle(t *testing.T) {
	s := gen.Shape{
		Tag:       "rectangle",
		Rectangle: &gen.Rectangle{Width: 10, Height: 20},
	}
	data, err := json.Marshal(s)
	if err != nil {
		t.Fatal(err)
	}
	var s2 gen.Shape
	if err := json.Unmarshal(data, &s2); err != nil {
		t.Fatal(err)
	}
	if s2.Tag != "rectangle" || s2.Rectangle.Width != 10 || s2.Rectangle.Height != 20 {
		t.Fatalf("oneof rectangle mismatch: got %+v", s2)
	}
}

func TestDrawingWithShapes(t *testing.T) {
	d := gen.Drawing{
		Name: "my_drawing",
		Shape: gen.Shape{
			Tag:    "circle",
			Circle: &gen.Circle{Radius: 3.0},
		},
		Shapes: []gen.Shape{
			{Tag: "circle", Circle: &gen.Circle{Radius: 1.0}},
			{Tag: "rectangle", Rectangle: &gen.Rectangle{Width: 5, Height: 10}},
			{Tag: "triangle", Triangle: &gen.Triangle{Base: 3, Height: 4, Label: "tri"}},
		},
	}
	data, err := json.Marshal(d)
	if err != nil {
		t.Fatal(err)
	}
	var d2 gen.Drawing
	if err := json.Unmarshal(data, &d2); err != nil {
		t.Fatal(err)
	}
	if d2.Name != "my_drawing" || len(d2.Shapes) != 3 {
		t.Fatalf("drawing roundtrip mismatch: got %+v", d2)
	}
	if d2.Shapes[2].Triangle.Label != "tri" {
		t.Fatalf("drawing triangle label mismatch: got %v", d2.Shapes[2].Triangle.Label)
	}
}

func TestPreciseInts(t *testing.T) {
	p := gen.PreciseInts{
		I8: -128, I16: -32768, I32: -2147483648, I64: -9223372036854775808,
		U8: 255, U16: 65535, U32: 4294967295, U64: 18446744073709551615,
	}
	data, err := json.Marshal(p)
	if err != nil {
		t.Fatal(err)
	}
	var p2 gen.PreciseInts
	if err := json.Unmarshal(data, &p2); err != nil {
		t.Fatal(err)
	}
	if p2.U64 != 18446744073709551615 || p2.I8 != -128 {
		t.Fatalf("precise ints mismatch: got %+v", p2)
	}
}

func TestCJsonCompatibility(t *testing.T) {
	// Test that Go can parse JSON produced by the C runtime
	cjson := `{"name":"Alice","age":"30"}`
	var p gen.Person
	if err := json.Unmarshal([]byte(cjson), &p); err != nil {
		t.Fatal(err)
	}
	if p.Name != "Alice" || p.Age != "30" {
		t.Fatalf("C JSON compat mismatch: got %+v", p)
	}
}

func TestOptionalFromPartialJson(t *testing.T) {
	partial := `{"id":1,"name":"test"}`
	var o gen.OptionalOnlyStruct
	if err := json.Unmarshal([]byte(partial), &o); err != nil {
		t.Fatal(err)
	}
	if o.Id != 1 || *o.Name != "test" {
		t.Fatalf("partial JSON mismatch: got %+v", o)
	}
	if o.Score != nil || o.Active != nil {
		t.Fatalf("missing optional fields should be nil: got %+v", o)
	}
}

func TestMapAllTypes(t *testing.T) {
	m := gen.MapAllTypesStruct{
		Int_map:    map[string]int32{"a": 1},
		Long_map:   map[string]int64{"b": 2},
		Float_map:  map[string]float32{"c": 3.0},
		Double_map: map[string]float64{"d": 4.0},
		Bool_map:   map[string]bool{"e": true},
		Str_map:    map[string]string{"f": "g"},
		Enum_map:   map[string]gen.Color{"h": gen.ColorRED},
		Struct_map: map[string]gen.Person{"i": {Name: "Bob", Age: "25"}},
	}
	data, err := json.Marshal(m)
	if err != nil {
		t.Fatal(err)
	}
	var m2 gen.MapAllTypesStruct
	if err := json.Unmarshal(data, &m2); err != nil {
		t.Fatal(err)
	}
	if m2.Struct_map["i"].Name != "Bob" {
		t.Fatalf("map struct roundtrip mismatch: got %+v", m2)
	}
}

func TestEdgeCaseStruct(t *testing.T) {
	e := gen.EdgeCaseStruct{
		Zero_int:             0,
		Negative_long:        -999999999999,
		Tiny_float:           0.000001,
		Huge_double:          1e308,
		False_bool:           false,
		Empty_string:         "",
		Special_chars_string: "hello \"world\"\nnewline\ttab",
	}
	data, err := json.Marshal(e)
	if err != nil {
		t.Fatal(err)
	}
	var e2 gen.EdgeCaseStruct
	if err := json.Unmarshal(data, &e2); err != nil {
		t.Fatal(err)
	}
	if e2.Special_chars_string != "hello \"world\"\nnewline\ttab" {
		t.Fatalf("edge case mismatch: got %+v", e2)
	}
}

func TestNullableNestedStruct(t *testing.T) {
	n := gen.NullableNestedStruct{
		Id:     1,
		Person: &gen.Person{Name: "Alice", Age: "30"},
		Color:  nil,
		Status: nil,
	}
	data, err := json.Marshal(n)
	if err != nil {
		t.Fatal(err)
	}
	var n2 gen.NullableNestedStruct
	if err := json.Unmarshal(data, &n2); err != nil {
		t.Fatal(err)
	}
	if n2.Person.Name != "Alice" || n2.Color != nil {
		t.Fatalf("nullable nested mismatch: got %+v", n2)
	}
}

// containsKey checks if a JSON string contains a given key.
func containsKey(jsonStr, key string) bool {
	return json.Valid([]byte(jsonStr)) && len(jsonStr) > 0 &&
		// Simple substring check for the key
		func() bool {
			needle := `"` + key + `"`
			for i := 0; i <= len(jsonStr)-len(needle); i++ {
				if jsonStr[i:i+len(needle)] == needle {
					return true
				}
			}
			return false
		}()
}
