// Pingus - A free Lemmings clone
// Copyright (C) 2007 Ingo Ruhnke <grumbel@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef HEADER_PINGUS_EDITOR_LABEL_HPP
#define HEADER_PINGUS_EDITOR_LABEL_HPP

#include "engine/gui/rect_component.hpp"

namespace Editor {

class EditorLabel : public GUI::RectComponent
{
private:
  std::string text;

public:
  EditorLabel(const Rect& rect, const std::string& text);
  EditorLabel(const std::string& text);

  bool is_at(int, int) { return false; }
  void draw (DrawingContext& gc);
  void update (float delta);
  void update_layout() {}

  std::string get_text() { return text; }
  void set_text(const std::string& text_) { text = text_; }
};

} // namespace Editor

#endif

/* EOF */
